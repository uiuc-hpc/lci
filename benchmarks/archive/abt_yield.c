/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "abt.h"

#define DEFAULT_NUM_XSTREAMS    4
#define DEFAULT_NUM_THREADS     4

#include "comm_exp.h"

void thread_func(void *arg)
{
  int i;
  for (i = 0; i < TOTAL; i++)
    ABT_thread_yield();
}

int main(int argc, char *argv[])
{
  ABT_init(argc, argv);

  int i, j;
  int ret;
  int num_xstreams = DEFAULT_NUM_XSTREAMS;
  int num_threads = DEFAULT_NUM_THREADS;
  if (argc > 1) num_xstreams = atoi(argv[1]);
  if (argc > 2) num_threads = atoi(argv[2]);

  int total_threads = num_threads * num_xstreams;

  ABT_xstream *xstreams;
  xstreams = (ABT_xstream *)malloc(sizeof(ABT_xstream) * (num_xstreams));

  ABT_pool *pools;
  pools = (ABT_pool *)malloc(sizeof(ABT_pool) * (num_xstreams));

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
  double t = wtime();

  for (i = 0; i < num_threads * num_xstreams; i++) {
    ret = ABT_thread_create(pools[i % num_xstreams],
        thread_func, (void*) (size_t) i, ABT_THREAD_ATTR_NULL,
        &thread[i]);
  }

  /* Switch to other user level threads */
  ABT_thread_yield();

  /* Join Execution Streams */
  for (i = 0; i < num_threads * num_xstreams; i++) {
    ABT_thread_join(thread[i]);
  }

  t = wtime() - t;
  printf("%f\n", 1e6 * t / total_threads / TOTAL);

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
