#include <iostream>
#include <atomic>
#include <thread>
#include <unistd.h>
#include "lci.h"
#include "comm_exp.h"

#undef MAX_MSG
#define MAX_MSG (8 * 1024)

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

int rank, nprocs, num_threads = NUM_THREADS;
int min_size = MIN_MSG;
int max_size = MAX_MSG;

int main(int argc, char *argv[])
{
  LCI_open();
  LCI_PL_t prop;
  LCI_PL_create(&prop);
  LCI_MT_t mt;
  LCI_MT_init(&mt, 0);
  LCI_PL_set_MT(prop, &mt);
  LCI_endpoint_create(0, prop, &ep);

  rank = LCI_RANK;
  nprocs = LCI_NUM_PROCESSES;

  auto prg_thread = std::thread([] {
      int spin = 64;
      while (!thread_stop) {
        LCI_progress(0, 1);
        if (spin-- == 0)
        { sched_yield(); spin = 64; }
      }
    });

  if(rank == 0) {
      thread_run(send_thread, num_threads);
      LCI_barrier();
  } else {
      thread_run(recv_thread, num_threads);
      LCI_barrier();
  }

  thread_stop = 1;
  prg_thread.join();
  LCI_close();
  return EXIT_SUCCESS;
}

void* recv_thread(void* arg)
{
  unsigned long align_size = sysconf(_SC_PAGESIZE);
  char * ret = NULL;
  char *buf;
  int val = thread_id();

  if (posix_memalign((void**)&buf, align_size, max_size)) {
    fprintf(stderr, "Error allocating host memory\n");
    *ret = '1';
    return ret;
  }

  for (int size = min_size; size <= max_size; size = (size ? size * 2 : 1)) {
    thread_barrier();
    if (thread_id() == 0)
      LCI_barrier();
    thread_barrier();

    LCI_syncl_t sync;
    thread_barrier();

    for (int i = 0; i < TOTAL; ++i) {
      LCI_one2one_set_empty(&sync);
      LCI_recvbc(buf, size, 1-rank, val, ep, &sync);
      while (LCI_one2one_test_empty(&sync)) continue;
      check_buffer(buf, size, 's');
      write_buffer(buf, size, 'r');
      while (LCI_sendbc(buf, size, 1-rank, val, ep) != LCI_OK) continue;
    }

    thread_barrier();
  }

  free(buf);
  return 0;
}


void* send_thread(void* arg)
{
  unsigned long align_size = sysconf(_SC_PAGESIZE);
  char *buf;
  char *ret = NULL;

  int val = thread_id();

  if (posix_memalign((void**)&buf, align_size, max_size)) {
    fprintf(stderr, "Error allocating host memory\n");
    *ret = '1';
    return ret;
  }

  char extra[256];
  if (val == 0) {
    sprintf(extra, "(%d, %d)", num_threads, num_threads);
  }

  for (int size = min_size; size <= max_size; size = (size ? size * 2 : 1)) {
    thread_barrier();
    if (thread_id() == 0)
      LCI_barrier();
    thread_barrier();

    LCI_syncl_t sync;
    thread_barrier();

    for (int i = 0; i < TOTAL; ++i) {
      write_buffer(buf, size, 's');
      while (LCI_sendbc(buf, size, 1-rank, val, ep) != LCI_OK) continue;

      LCI_one2one_set_empty(&sync);
      LCI_recvbc(buf, size, 1-rank, val, ep, &sync);
      while (LCI_one2one_test_empty(&sync)) continue;
      check_buffer(buf, size, 'r');
    }

    thread_barrier();
  }

  free(buf);
  return 0;
}
