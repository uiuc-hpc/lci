#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <vector>
#include <chrono>
#include <algorithm>
#include <thread>
#include <atomic>
#include <pthread.h>
#include <mutex>
#include <condition_variable>
#include <getopt.h>
#include <random>
#include <unistd.h>

#include "lci.h"

using namespace std;

const size_t PAGESIZE = sysconf(_SC_PAGESIZE);

struct Config {
  int inject_rate = 1e6;
  int nmsgs = 2e3;
  int nmsgs_std = 0;
  int msg_size = 2048;
  int nthreads = 4;
  int verbose = 0;
} config;

// Define a struct with size 64 byte (prevent false sharing)
template <typename T>
struct alignas(64) alignedAtomic {
  atomic<T> data;
  char padding[64 - sizeof(atomic<T>)];
  alignedAtomic() : data(0) {}
  explicit alignedAtomic(T input) : data(input) {}
};

// Number of msg to be sent to each process
vector<alignedAtomic<int>>* sendCounts;
// Records number of msg recved from each process
vector<alignedAtomic<int>>* msgRecved;
// Records number of msg sent from each process
vector<alignedAtomic<int>>* msgSent;
// Message received in this process
alignas(64) atomic<int> nRecv(0);
// When to stop the progress thread
alignas(64) atomic<bool> progressStop(false);
// Padding between shared variables that can be modified and those that cannot.
char g_padding[64];
int expectedRecv = 0;
LCI_endpoint_t g_ep;
LCI_comp_t g_cq;
LCT_tbarrier_t barrier;

void write_buffer(char* buf, size_t size, unsigned int seed)
{
  for (int i = 0; i < size; ++i) {
    buf[i] = static_cast<char>(rand_r(&seed));
  }
}

void check_buffer(const char* buf, size_t size, unsigned int seed)
{
  for (int i = 0; i < size; ++i) {
    char expected = static_cast<char>(rand_r(&seed));
    if (buf[i] != expected) {
      fprintf(stderr, "%d: check_buffer failed! (buf[%d] = %u != %u\n",
              LCI_RANK, i, buf[i], expected);
      abort();
    }
  }
}

void global_barrier(int thread_id)
{
  LCT_tbarrier_arrive_and_wait(barrier);
  if (thread_id == 0) LCI_barrier();
  LCT_tbarrier_arrive_and_wait(barrier);
}

// Manages send completion status for all other threads
void progressFcn()
{
  while (!progressStop) {
    LCI_progress(LCI_UR_DEVICE);
  }
}

void threadFcn(int thread_id)
{
  // Block the thread until all threads are created
  LCT_tbarrier_arrive_and_wait(barrier);

  // Temporarily store send and recv number locally
  int l_nsends = 0;
  int l_nrecvs = 0;
  std::vector<int> l_msgSent(LCI_NUM_PROCESSES, 0);
  std::vector<int> l_msgRecved(LCI_NUM_PROCESSES, 0);

  // Buffers
  LCI_mbuffer_t send_buffer, recv_buffer;
  int ret;
  ret = posix_memalign(&send_buffer.address, PAGESIZE, config.msg_size);
  assert(ret == 0);
  send_buffer.length = config.msg_size;

  unsigned int seed = LCT_now() + LCI_RANK + thread_id;
  global_barrier(thread_id);
  auto start_time = LCT_now();
  // the send loop
  while (true) {
    if (config.inject_rate &&
        l_nsends / LCT_time_to_us(LCT_now() - start_time) > config.inject_rate)
      // We are sending too fast!
      continue;

    // Attempt to find a valid send destination
    int dest = (rand_r(&seed) % LCI_NUM_PROCESSES) - 1;

    bool found = false;
    for (int i = 0; i < LCI_NUM_PROCESSES; ++i) {
      dest = (dest + 1) % LCI_NUM_PROCESSES;
      int sendCount = --((*sendCounts)[dest].data);
      if (sendCount >= 0) {
        found = true;
        break;
      }
    }

    if (!found) {
      // All destination are done sending!
      break;
    }

    // send a message
    unsigned int data_seed = LCI_RANK + thread_id + l_nsends;
    write_buffer((char*)send_buffer.address, send_buffer.length, data_seed);
    while (LCI_putma(g_ep, send_buffer, dest, data_seed,
                     LCI_DEFAULT_COMP_REMOTE) == LCI_ERR_RETRY)
      continue;

    // Update send counts
    l_nsends++;
    l_msgSent[dest]++;

    // Receive message
    if (nRecv < expectedRecv) {
      LCI_request_t request;
      if (LCI_queue_pop(g_cq, &request) == LCI_OK) {
        l_nrecvs++;
        l_msgRecved[request.rank]++;
        check_buffer((char*)request.data.mbuffer.address,
                     request.data.mbuffer.length, request.tag);
        LCI_mbuffer_free(request.data.mbuffer);
      }
    }
  }
  nRecv += l_nrecvs;

  // Wait for recv to complete
  while (nRecv < expectedRecv) {
    LCI_request_t request;
    if (LCI_queue_pop(g_cq, &request) == LCI_OK) {
      nRecv++;
      l_msgRecved[request.rank]++;
      check_buffer((char*)request.data.mbuffer.address,
                   request.data.mbuffer.length, request.tag);
      LCI_mbuffer_free(request.data.mbuffer);
    }
  }

  global_barrier(thread_id);
  auto total_time = LCT_now() - start_time;

  // Update global send and recv counts
  for (int i = 0; i < LCI_NUM_PROCESSES; i++) {
    (*msgSent)[i].data += l_msgSent[i];
    (*msgRecved)[i].data += l_msgRecved[i];
  }
  LCT_tbarrier_arrive_and_wait(barrier);

  if (config.verbose && thread_id == 0) {
    // Message count for correctness
    for (int r = 0; r < LCI_NUM_PROCESSES; r++) {
      printf("process %d sent to process %d: %d \n", LCI_RANK, r,
             (*msgSent)[r].data.load());
      printf("process %d received from process %d: %d \n", LCI_RANK, r,
             (*msgRecved)[r].data.load());
    }
  }
  if (LCI_RANK == 0 && thread_id == 0) {
    // Total time to send and recv messages
    printf("Total time: %lf s \n", LCT_time_to_s(total_time));
  }
}

int main(int argc, char** argv)
{
  // Initialize LCI data structure
  LCI_initialize();
  LCI_queue_create(LCI_UR_DEVICE, &g_cq);
  LCI_plist_t plist;
  LCI_plist_create(&plist);
  LCI_plist_set_comp_type(plist, LCI_PORT_COMMAND, LCI_COMPLETION_QUEUE);
  LCI_plist_set_comp_type(plist, LCI_PORT_MESSAGE, LCI_COMPLETION_QUEUE);
  LCI_plist_set_match_type(plist, LCI_MATCH_RANKTAG);
  LCI_plist_set_default_comp(plist, g_cq);
  LCI_endpoint_init(&g_ep, LCI_UR_DEVICE, plist);
  LCI_plist_free(&plist);

  // Parse arguments
  LCT_args_parser_t argsParser = LCT_args_parser_alloc();
  LCT_args_parser_add(argsParser, "nthreads", required_argument,
                      &config.nthreads);
  LCT_args_parser_add(argsParser, "nmsgs", required_argument, &config.nmsgs);
  LCT_args_parser_add(argsParser, "nmsgs-std", required_argument,
                      &config.nmsgs_std);
  LCT_args_parser_add(argsParser, "size", required_argument, &config.msg_size);
  LCT_args_parser_add(argsParser, "rate", required_argument,
                      &config.inject_rate);
  LCT_args_parser_add(argsParser, "verbose", required_argument,
                      &config.verbose);
  LCT_args_parser_parse(argsParser, argc, argv);
  if (LCI_RANK == 0) LCT_args_parser_print(argsParser, true);
  LCT_args_parser_free(argsParser);

  // Initialize global variables
  msgSent = new vector<alignedAtomic<int>>(LCI_NUM_PROCESSES);
  msgRecved = new vector<alignedAtomic<int>>(LCI_NUM_PROCESSES);
  sendCounts = new vector<alignedAtomic<int>>(LCI_NUM_PROCESSES);
  for (int i = 0; i < LCI_NUM_PROCESSES; i++) {
    (*msgRecved)[i].data.store(0);
    (*msgSent)[i].data.store(0);
  }
  barrier = LCT_tbarrier_alloc(config.nthreads);

  // Generate and exchange number of message to send to each process
  unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
  std::default_random_engine generator(seed);
  std::normal_distribution<double> distribution(config.nmsgs, config.nmsgs_std);
  for (int i = 0; i < LCI_NUM_PROCESSES; i++) {
    (*sendCounts)[i].data = static_cast<int>(distribution(generator));
  }
  for (int i = 0; i < LCI_NUM_PROCESSES; i++) {
    LCI_short_t src;
    *(int*)&src = (*sendCounts)[i].data;
    while (LCI_puts(g_ep, src, i % LCI_NUM_PROCESSES, 0,
                    LCI_DEFAULT_COMP_REMOTE) == LCI_ERR_RETRY) {
      LCI_progress(LCI_UR_DEVICE);
    }
  }
  for (int i = 0; i < LCI_NUM_PROCESSES; ++i) {
    LCI_request_t request;
    while (LCI_queue_pop(g_cq, &request) == LCI_ERR_RETRY) {
      LCI_progress(LCI_UR_DEVICE);
    }
    expectedRecv += *(int*)&request.data.immediate;
  }

  // Progress thread
  thread progresser(progressFcn);

  // Create threads to send and recv
  vector<thread> workers;
  for (int i = 0; i < config.nthreads; i++) {
    workers.emplace_back(threadFcn, i);
  }

  // Signal all threads to start, record time
  for (auto& worker : workers) {
    worker.join();
  }

  progressStop = true;
  progresser.join();

  // Finalize LCI environment
  LCI_finalize();
}