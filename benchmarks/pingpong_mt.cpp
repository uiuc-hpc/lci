#include <iostream>
#include <atomic>
#include <thread>
#include <unistd.h>
#include <assert.h>
#include "lci.h"

#define USE_THREAD
#include "comm_exp.h"

/**
 * Multithreaded ping-pong benchmark with sendbc/recvbc
 */

void * send_thread(void*);
void * recv_thread(void*);

static int thread_stop = 0;
LCI_endpoint_t ep;

int rank, peer_rank, nranks, num_threads;
int min_threads = 1;
int max_threads = 4;
int min_size = 8;
int max_size = 64;
bool touch_data = false;

int main(int argc, char *argv[])
{
  if (argc > 1)
    min_threads = atoi(argv[1]);
  if (argc > 2)
    max_threads = atoi(argv[2]);
  if (argc > 3)
    min_size = atoi(argv[3]);
  if (argc > 4)
    max_size = atoi(argv[4]);
  if (argc > 5)
    touch_data = atoi(argv[5]);

  LCI_initialize();
  ep = LCI_UR_ENDPOINT;

  rank = LCI_RANK;
  nranks = LCI_NUM_PROCESSES;
  peer_rank = (rank + nranks / 2) % nranks;
  yp_init();

  std::atomic<int> started = {0};
  auto prg_thread = std::thread([&started] {
    int spin = 64;
    int core = 0;
    if (getenv("LC_SCORE"))
      core = atoi(getenv("LC_SCORE"));
    comm_set_me_to(core); // only for hyper-threaded. FIXME.
    started++;
    while (!thread_stop) {
      LCI_progress(LCI_UR_DEVICE);
      if (spin-- == 0)
      { sched_yield(); spin = 64; }
    }
//      printf( "rank %d prg thread %2d of %2d running on cpu %2d!\n",
//              rank, i+1, NUM_DEV, sched_getcpu());
  });

  if (rank < nranks / 2 ) {
    print_banner();

    for (num_threads=min_threads; num_threads <= max_threads; num_threads *= 2)
    {
      omp::thread_run(send_thread, num_threads);
      LCI_barrier();
    }
  } else {
    for (num_threads=min_threads; num_threads <= max_threads; num_threads *= 2)
    {
      omp::thread_run(recv_thread, num_threads);
      LCI_barrier();
    }
  }

  thread_stop = 1;
  prg_thread.join();
  LCI_finalize();
  return EXIT_SUCCESS;
}

void* send_thread(void* arg)
{
//  printf("recv thread %d/%d on rank %d/%d\n", thread_id(), thread_count(), rank, size);
  int thread_id = omp::thread_id();
  int thread_count = omp::thread_count();
  LCI_comp_t cq;
  LCI_queue_create(0, &cq);

  LCI_mbuffer_t mbuffer;
  LCI_request_t request;
  LCI_tag_t tag = 99 + thread_id;

  for (int size = min_size; size <= max_size; size *= 2) {
    omp::thread_barrier();
    if (thread_id == 0)
      LCI_barrier();
    omp::thread_barrier();

    RUN_VARY_MSG({size, size}, 0, [&](int msg_size, int iter) {
      while (LCI_mbuffer_alloc(LCI_UR_DEVICE, &mbuffer) == LCI_ERR_RETRY)
        continue;
      if (touch_data) write_buffer((char*) mbuffer.address, msg_size, 's');
      mbuffer.length = msg_size;
      while (LCI_sendmn(ep, mbuffer, peer_rank, tag) == LCI_ERR_RETRY)
        continue;

      LCI_recvmn(ep, peer_rank, tag, cq, NULL);
      while (LCI_queue_pop(cq, &request) == LCI_ERR_RETRY)
        continue;
      assert(request.data.mbuffer.length == msg_size);
      if (touch_data) check_buffer((char*) request.data.mbuffer.address, msg_size, 's');
      LCI_mbuffer_free(request.data.mbuffer);
    }, {rank % (size / 2) * thread_count + thread_id, (size / 2) * thread_count});

    omp::thread_barrier();
  }

//  printf( "rank %d omp thread %2d of %2d running on cpu %2d!\n",
//          rank,
//          omp_get_thread_num()+1,
//          omp_get_num_threads(),
//          sched_getcpu());
  LCI_queue_free(&cq);
  return 0;
}


void* recv_thread(void* arg)
{
//  printf("send thread %d/%d on rank %d/%d\n", thread_id(), thread_count(), rank, size);
  int thread_id = omp::thread_id();
  int thread_count = omp::thread_count();
  LCI_comp_t cq;
  LCI_queue_create(0, &cq);

  LCI_mbuffer_t mbuffer;
  LCI_request_t request;
  LCI_tag_t tag = 99 + thread_id;

  for (int size = min_size; size <= max_size; size *= 2) {
    omp::thread_barrier();
    if (thread_id == 0)
      LCI_barrier();
    omp::thread_barrier();

    RUN_VARY_MSG({size, size}, 1, [&](int msg_size, int iter) {
      LCI_recvmn(ep, peer_rank, tag, cq, NULL);
      while (LCI_queue_pop(cq, &request) == LCI_ERR_RETRY)
        continue;
      assert(request.data.mbuffer.length == msg_size);
      if (touch_data) check_buffer((char*) request.data.mbuffer.address, msg_size, 's');
      LCI_mbuffer_free(request.data.mbuffer);

      while (LCI_mbuffer_alloc(LCI_UR_DEVICE, &mbuffer) == LCI_ERR_RETRY)
        continue;
      if (touch_data) write_buffer((char*) mbuffer.address, msg_size, 's');
      mbuffer.length = msg_size;
      while (LCI_sendmn(ep, mbuffer, peer_rank, tag) == LCI_ERR_RETRY)
        continue;

    }, {rank % (size / 2) * thread_count + thread_id, (size / 2) * thread_count});

    omp::thread_barrier();
  }

//  printf( "rank %d omp thread %2d of %2d running on cpu %2d!\n",
//          rank,
//          omp_get_thread_num()+1,
//          omp_get_num_threads(),
//          sched_getcpu());
  LCI_queue_free(&cq);
  return 0;
}
