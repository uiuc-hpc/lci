#include <iostream>
#include <atomic>
#include <thread>
#include <unistd.h>
#include "lci.h"

#define YP_USE_THREAD
#include "comm_exp.h"


void * send_thread(void*);
void * recv_thread(void*);

const int NUM_DEV = 1;
#define GETDEV(tag) (tag & (NUM_DEV-1))
static int thread_stop = 0;
LCI_endpoint_t ep[NUM_DEV];

int rank, size, num_threads = 0;
int min_threads = 1;
int max_threads = 4;
int min_size = 8;
int max_size = 64;

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

  LCI_Init(NULL, NULL);
  LCI_PL_t prop;
  LCI_PL_create(&prop);
  LCI_MT_t mt;
  LCI_MT_init(&mt, 0);
  LCI_PL_set_MT(prop, &mt);
  LCI_endpoint_create(0, prop, &ep[0]);
  for (int i = 1; i < NUM_DEV; i++) {
    LCI_endpoint_create(i, prop, &ep[i]);
  }

  rank = LCI_RANK;
  size = LCI_NUM_PROCESSES;

  std::atomic<int> started = {0};
  for (int i = 0; i < NUM_DEV; i++) {
    std::thread([i, &started] {
      int spin = 64;
      int core = 0;
      if (getenv("LC_SCORE"))
        core = atoi(getenv("LC_SCORE"));
      comm_set_me_to(core + 2*i); // only for hyper-threaded. FIXME.
      started++;
      while (!thread_stop) {
        LCI_progress(i, 1);
        if (spin-- == 0)
        { sched_yield(); spin = 64; }
      }
    }).detach();
  }
  while (started.load() != NUM_DEV) continue;

  thread_init();

  int i = 0;

  if(rank == 0) {
    print_banner();

    for (num_threads=min_threads; num_threads <= max_threads; num_threads *= 2)
//      for (num_threads=min_threads; num_threads <= max_threads; num_threads += (num_threads > 1 ? 2 : 1))
    {
      thread_run(send_thread, num_threads);
      LCI_PM_barrier();
    }
  } else {
    for (num_threads=min_threads; num_threads <= max_threads; num_threads *= 2)
    {
      thread_run(recv_thread, num_threads);
      LCI_PM_barrier();
    }
  }

  thread_stop = 1;
  LCI_Free();
  return EXIT_SUCCESS;
}

void* recv_thread(void* arg)
{
//  printf("recv thread %d/%d on rank %d/%d\n", thread_id(), thread_count(), rank, size);
  unsigned long align_size = sysconf(_SC_PAGESIZE);
  int size, i, val;
  char * ret = NULL;
  char *s_buf, *r_buf;
  val = thread_id();

  if (_memalign((void**)&s_buf, align_size, max_size)) {
    fprintf(stderr, "Error allocating host memory\n");
    *ret = '1';
    return ret;
  }

  if (_memalign((void**)&r_buf, align_size, max_size)) {
    fprintf(stderr, "Error allocating host memory\n");
    *ret = '1';
    return ret;
  }

  for (size = min_size; size <= max_size; size = (size ? size * 2 : 1)) {
    thread_barrier();
    if (thread_id() == 0)
      LCI_PM_barrier();
    thread_barrier();

    /* touch the data */
    for(i = 0; i < size; i++) {
      s_buf[i] = 'a';
      r_buf[i] = 'b';
    }
    LCI_syncl_t sync;

    thread_barrier();

    RUN_VARY_MSG({size, size}, 0, [&](int msg_size, int iter) {
      LCI_one2one_set_empty(&sync);
      LCI_recvbc(r_buf, size, 1-rank, val, ep[GETDEV(val)], &sync);
      while (LCI_one2one_test_empty(&sync))
        LCI_progress(0, 1);

      while (LCI_sendbc(s_buf, size, 1-rank, val, ep[GETDEV(val)]) != LCI_OK)
        LCI_progress(0, 1);
    }, {val, num_threads});

    thread_barrier();
  }

  _free(r_buf);
  _free(s_buf);

  return 0;
}


void* send_thread(void* arg)
{
//  printf("send thread %d/%d on rank %d/%d\n", thread_id(), thread_count(), rank, size);
  unsigned long align_size = sysconf(_SC_PAGESIZE);
  int size, i, val;
  char *s_buf, *r_buf;
  double t_start = 0, t_end = 0, t = 0, latency;
  char *ret = NULL;

  val = thread_id();

  if (_memalign((void**)&s_buf, align_size, max_size)) {
    fprintf(stderr, "Error allocating host memory\n");
    *ret = '1';
    return ret;
  }

  if (_memalign((void**)&r_buf, align_size, max_size)) {
    fprintf(stderr, "Error allocating host memory\n");
    *ret = '1';
    return ret;
  }

  char extra[256];
  if (val == 0) {
    sprintf(extra, "(%d, %d)", num_threads, num_threads);
  }

  for (size = min_size; size <= max_size; size = (size ? size * 2 : 1)) {
    thread_barrier();
    if (thread_id() == 0)
      LCI_PM_barrier();
    thread_barrier();

    /* touch the data */
    for(i = 0; i < size; i++) {
      s_buf[i] = 'a';
      r_buf[i] = 'b';
    }
    LCI_syncl_t sync;

    thread_barrier();

    RUN_VARY_MSG({size, size}, 1, [&](int msg_size, int iter) {
      while (LCI_sendbc(s_buf, size, 1-rank, val, ep[GETDEV(val)]) != LCI_OK)
        LCI_progress(0, 1);

      LCI_one2one_set_empty(&sync);
      LCI_recvbc(r_buf, size, 1-rank, val, ep[GETDEV(val)], &sync);
      while (LCI_one2one_test_empty(&sync))
        LCI_progress(0, 1);
    }, {val, num_threads}, extra);

    thread_barrier();
  }

  _free(r_buf);
  _free(s_buf);

  return 0;
}
