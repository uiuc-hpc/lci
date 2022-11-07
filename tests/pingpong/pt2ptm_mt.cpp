#include <iostream>
#include <atomic>
#include <thread>
#include <unistd.h>
#include "lci.h"
#include "comm_exp.h"

#undef MAX_MSG
#define MAX_MSG LCI_MEDIUM_SIZE
#undef TOTAL
#define TOTAL 100
/**
 * Multithreaded ping-pong test with sendbc/recvbc
 * Touch the data
 *
 * write the buffer before send and read the buffer after recv
 */
using namespace omp;

void * send_thread(void*);
void * recv_thread(void*);

static int thread_stop = 0;
LCI_endpoint_t ep;

int rank, nprocs, peer_rank, num_threads = NUM_THREADS;
int min_size = MIN_MSG;
int max_size = MAX_MSG;

int main(int argc, char *argv[])
{
  LCI_initialize();
  LCI_plist_t plist;
  LCI_plist_create(&plist);
  LCI_plist_set_comp_type(plist, LCI_PORT_MESSAGE, LCI_COMPLETION_SYNC);
  LCI_endpoint_init(&ep, LCI_UR_DEVICE, plist);

  rank = LCI_RANK;
  printf("%d start!\n", rank);
  nprocs = LCI_NUM_PROCESSES;
  peer_rank = ((1 - LCI_RANK % 2) + LCI_RANK / 2 * 2) % LCI_NUM_PROCESSES;

  auto prg_thread = std::thread([] {
      int spin = 64;
      while (!thread_stop) {
        LCI_progress(LCI_UR_DEVICE);
        if (spin-- == 0)
        { sched_yield(); spin = 64; }
      }
    });

  if(rank % 2 == 0) {
      thread_run(send_thread, num_threads);
      LCI_barrier();
  } else {
      thread_run(recv_thread, num_threads);
      LCI_barrier();
  }

  thread_stop = 1;
  prg_thread.join();
  LCI_finalize();
  return EXIT_SUCCESS;
}

void* recv_thread(void* arg)
{
  int tag = thread_id();

  LCI_comp_t sync;
  LCI_sync_create(LCI_UR_DEVICE, 1, &sync);

  size_t alignment = sysconf(_SC_PAGESIZE);
  LCI_mbuffer_t src_buf, dst_buf;
  posix_memalign(&src_buf.address, alignment, MAX_MSG);
  posix_memalign(&dst_buf.address, alignment, MAX_MSG);

  for (int size = min_size; size <= max_size; size = (size ? size * 2 : 1)) {
    src_buf.length = size;
    dst_buf.length = size;

    thread_barrier();
    if (thread_id() == 0)
      LCI_barrier();
    thread_barrier();

    for (int i = 0; i < TOTAL; ++i) {
      write_buffer((char*)src_buf.address, size, 's');
      write_buffer((char*)dst_buf.address, size, 'r');


      LCI_recvm(ep, dst_buf, peer_rank, tag, sync, nullptr);
      while (LCI_sync_test(sync, NULL) == LCI_ERR_RETRY) continue;
      check_buffer((char*)dst_buf.address, size, 's');

      while (LCI_sendm(ep, src_buf, peer_rank, tag) != LCI_OK) continue;
      if (i % 1000 == 0) {
        printf("size %d iter %d done\n", size, i);
      }
    }

    thread_barrier();
  }

  free(src_buf.address);
  free(dst_buf.address);
  return nullptr;
}


void* send_thread(void* arg)
{
  int tag = thread_id();

  LCI_comp_t sync;
  LCI_sync_create(LCI_UR_DEVICE, 1, &sync);

  size_t alignment = sysconf(_SC_PAGESIZE);
  LCI_mbuffer_t src_buf, dst_buf;
  posix_memalign(&src_buf.address, alignment, MAX_MSG);
  posix_memalign(&dst_buf.address, alignment, MAX_MSG);

  for (int size = min_size; size <= max_size; size = (size ? size * 2 : 1)) {
    src_buf.length = size;
    dst_buf.length = size;

    thread_barrier();
    if (thread_id() == 0)
      LCI_barrier();
    thread_barrier();

    for (int i = 0; i < TOTAL; ++i) {
      write_buffer((char*)src_buf.address, size, 's');
      write_buffer((char*)dst_buf.address, size, 'r');

      while (LCI_sendm(ep, src_buf, peer_rank, tag) != LCI_OK) continue;


      LCI_recvm(ep, dst_buf, peer_rank, tag, sync, nullptr);
      while (LCI_sync_test(sync, NULL) == LCI_ERR_RETRY) continue;
      check_buffer((char*)dst_buf.address, size, 's');
    }

    thread_barrier();
  }

  free(src_buf.address);
  free(dst_buf.address);
  return nullptr;
}
