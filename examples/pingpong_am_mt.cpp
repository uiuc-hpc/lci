#include <iostream>
#include <thread>
#include <cassert>
#include <chrono>
#include <atomic>
#include "lct.h"
#include "lci.hpp"

const int nthreads = 4;
const int nmsgs = 1000;
const size_t msg_size = 8;

LCT_tbarrier_t thread_barrier;
std::atomic<int> thread_seqence_control(0);

void worker(int thread_id)
{
  int rank = lci::get_rank();
  int nranks = lci::get_nranks();
  int peer_rank;
  if (nranks == 1) {
    peer_rank = rank;
  } else {
    peer_rank = (rank + nranks / 2) % nranks;
  }
  // allocate resouces
  // device and rcomp allocation needs to be synchronized to ensure uniformity
  // across ranks.
  while (thread_seqence_control != thread_id) continue;
  lci::comp_t cq = lci::alloc_cq();
  lci::rcomp_t rcomp = lci::register_rcomp(cq);
  lci::device_t device = lci::alloc_device();
  if (++thread_seqence_control == nthreads) thread_seqence_control = 0;

  void* send_buf = malloc(msg_size);
  memset(send_buf, rank, msg_size);

  LCT_tbarrier_arrive_and_wait(thread_barrier);
  auto start = std::chrono::high_resolution_clock::now();
  if (nranks == 1 || rank < nranks / 2) {
    // sender
    for (int i = 0; i < nmsgs; i++) {
      // send a message
      lci::post_am_x(peer_rank, send_buf, msg_size, lci::COMP_NULL_EXPECT_OK,
                     rcomp)
          .device(device)
          .tag(thread_id)();
      // wait for an incoming message
      lci::status_t status;
      do {
        lci::progress_x().device(device)();
        status = lci::cq_pop(cq);
      } while (status.error.is_retry());
      if (status.tag != thread_id) {
        std::cerr << "thread_id: " << thread_id
                  << ", status.tag: " << status.tag << std::endl;
      }
      assert(status.tag == thread_id);
      lci::buffer_t recv_buf = status.data.get_buffer();
      assert(recv_buf.size == msg_size);
      for (size_t j = 0; j < msg_size; j++) {
        assert(((char*)recv_buf.base)[j] == peer_rank);
      }
      free(recv_buf.base);
    }
  } else {
    // receiver
    for (int i = 0; i < nmsgs; i++) {
      // wait for an incoming message
      lci::status_t status;
      do {
        lci::progress_x().device(device)();
        status = lci::cq_pop(cq);
      } while (status.error.is_retry());
      assert(status.tag == thread_id);
      lci::buffer_t recv_buf = status.data.get_buffer();
      assert(recv_buf.size == msg_size);
      for (size_t j = 0; j < msg_size; j++) {
        assert(((char*)recv_buf.base)[j] == peer_rank);
      }
      free(recv_buf.base);
      // send a message
      lci::post_am_x(peer_rank, send_buf, msg_size, lci::COMP_NULL_EXPECT_OK,
                     rcomp)
          .device(device)
          .tag(thread_id)();
    }
  }
  LCT_tbarrier_arrive_and_wait(thread_barrier);
  auto end = std::chrono::high_resolution_clock::now();
  if (thread_id == 0 && rank == 0) {
    std::cout << "pingpong_am_mt: " << std::endl;
    std::cout << "Number of threads: " << nthreads << std::endl;
    std::cout << "Number of messages: " << nmsgs << std::endl;
    std::cout << "Message size: " << msg_size << " bytes" << std::endl;
    std::cout << "Number of ranks: " << nranks << std::endl;
    double total_time_us =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();
    double msg_rate_uni =
        1.0 * nmsgs * nthreads * (nranks + 1) / 2 / total_time_us;
    double bandwidth_uni = msg_rate_uni * msg_size;
    std::cout << "Total time: " << total_time_us / 1e6 << " s" << std::endl;
    std::cout << "Message rate: " << msg_rate_uni << " mmsg/s" << std::endl;
    std::cout << "Bandwidth: " << bandwidth_uni << " MB/s" << std::endl;
  }

  free(send_buf);
  // free resouces
  lci::free_comp(&cq);

  while (thread_seqence_control != thread_id) {
    lci::progress_x().device(device)();
  }
  lci::free_device(&device);
  if (++thread_seqence_control == nthreads) thread_seqence_control = 0;
}

int main(int argc, char** args)
{
  // Initialize the global default runtime.
  // Here we use the *objectized flexible function* version of the
  // `g_runtime_init` operation and specify that the default device should not
  // be allocated.
  lci::g_runtime_init_x().alloc_default_device(false)();
  // After at least one runtime is active, we can query the rank and nranks.
  // rank is the id of the current process
  // nranks is the total number of the processes in the current job.
  assert(lci::get_nranks() == 1 || lci::get_nranks() % 2 == 0);

  // get a thread barrier
  thread_barrier = LCT_tbarrier_alloc(nthreads);

  // spawn the threads to do the pingpong
  if (nthreads == 1) {
    worker(0);
  } else {
    std::vector<std::thread> threads;
    for (int i = 0; i < nthreads; i++) {
      threads.push_back(std::thread(worker, i));
    }
    for (auto& thread : threads) {
      thread.join();
    }
  }

  // free the thread barrier
  LCT_tbarrier_free(&thread_barrier);

  // Finalize the global default runtime
  lci::g_runtime_fina();
  return 0;
}
