/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>                // for gettimeofday()

#include "abt.h"

#define DEFAULT_NUM_XSTREAMS    4
#define DEFAULT_NUM_THREADS     4

#include "comm_exp.h"

double *times;

ABT_mutex* mutex;
ABT_cond* cond;

volatile int total = 0;

void thread_func(void *arg)
{
  size_t myid = (size_t) arg;
  ABT_mutex_lock(mutex[myid]);
  __sync_fetch_and_add(&total, 1);
  ABT_cond_wait(cond[myid], mutex[myid]);
  ABT_mutex_unlock(mutex[myid]);
}

int compare (const void * a, const void * b) { return (*(long*) a - *(long*) b); }

int main(int argc, char *argv[])
{
  ABT_init(argc, argv);

  int i, j;
  int ret;
  int num_xstreams = DEFAULT_NUM_XSTREAMS;
  int num_threads = DEFAULT_NUM_THREADS;
  num_xstreams = 2;
  if (argc > 2) num_threads = atoi(argv[2]);

  int total_threads = num_threads * num_xstreams;
  times = malloc(sizeof(double) * total_threads);
  memset(times, 0, sizeof(double) * total_threads);

  ABT_xstream *xstreams;
  xstreams = (ABT_xstream *)malloc(sizeof(ABT_xstream) * (num_xstreams + 1));

  ABT_pool *pools;
  pools = (ABT_pool *)malloc(sizeof(ABT_pool) * (num_xstreams + 1));

  /* Initialize */

  /* Create Execution Streams */
  ret = ABT_xstream_self(&xstreams[0]);
  for (i = 1; i < num_xstreams; i++) {
    ret = ABT_xstream_create(ABT_SCHED_NULL, &xstreams[i]);
  }

  /* Get the pools attached to an execution stream */
  for (i = 0; i < num_xstreams; i++) {
    ret = ABT_xstream_get_main_pools(xstreams[i], 1, pools+i);
  }

  ABT_thread thread[num_threads * num_xstreams];

  mutex = malloc(num_threads * num_xstreams * sizeof(ABT_mutex));
  cond = malloc(num_threads * num_xstreams * sizeof(ABT_cond));

  for (i = 0; i < num_threads * num_xstreams; i++) {
    /* Create a mutex */
    ret = ABT_mutex_create(&mutex[i]);

    /* Create condition variables */
    ret = ABT_cond_create(&cond[i]);
  }

  double t = 0;
  int time;
  for (time = 0; time < TOTAL_LARGE; time ++) {
    total = 0;
    /* Create threads */
    for (i = 0; i < num_threads * num_xstreams; i++) {
      ret = ABT_thread_create(pools[1],
          thread_func, (void*) (size_t) i, ABT_THREAD_ATTR_NULL,
          &thread[i]);
    }

    while (total != num_threads * num_xstreams) ABT_thread_yield();
    t-=wtime();
    for (i = 0; i < num_threads * num_xstreams; i++) {
      ABT_mutex_lock(mutex[i]);
      ret = ABT_cond_signal(cond[i]);
      ABT_mutex_unlock(mutex[i]);
    }

    /* Join Execution Streams */
    for (i = 0; i < num_threads * num_xstreams; i++) {
      ABT_thread_join(thread[i]);
    }
    t+=wtime();
  }

  printf("%f\n", 1e6 * t / total_threads /  TOTAL_LARGE);

  /* Free Execution Streams */
  for (i = 1; i < num_xstreams; i++) {
    ret = ABT_xstream_free(&xstreams[i]);
  }

  ABT_finalize();
  /* Finalize */
  free(pools);
  free(xstreams);

  return ret;
}
