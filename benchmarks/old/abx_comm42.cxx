/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abt.h"
#include <algorithm>
#include <math.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "comm_exp.h"

#define DEFAULT_NUM_XSTREAMS 4
#define DEFAULT_NUM_THREADS 4

static int SIZE = 1;

inline unsigned long long cycle_time()
{
  unsigned hi, lo;
  __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
  unsigned long long cycles =
      ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
  return cycles;
}

long* times;

ABT_mutex* mutex;
ABT_cond* cond;
double *start, *end;

#include "abt_sync.h"

#define USING_ABT
typedef ABT_sync MPIV_Request;
#include "mpiv.h"

volatile int total = 0;

void* alldata;

void thread_func(void* arg)
{
  size_t myid = (size_t)arg;
  void* buffer = (void*)((uintptr_t)alldata + SIZE * myid);
  start[myid] = MPI_Wtime();
  MPIV_Recv(buffer, SIZE, 1, myid);
  end[myid] = MPI_Wtime();
}

int main(int argc, char* argv[])
{
  int provide;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provide);
  ABT_init(argc, argv);

  int i = 0;
  int ret = 0;

  int num_xstreams = atoi(argv[1]);
  int num_threads = atoi(argv[2]);

  ABT_xstream* xstreams;
  xstreams = (ABT_xstream*)malloc(sizeof(ABT_xstream) * (num_xstreams + 1));

  ABT_pool* pools;
  pools = (ABT_pool*)malloc(sizeof(ABT_pool) * (num_xstreams + 1));

  start = (double*)malloc(sizeof(double) * (num_xstreams * num_threads));
  end = (double*)malloc(sizeof(double) * (num_xstreams * num_threads));

  /* Initialize */
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  if (rank == 0) {
    alldata = malloc(1 * 1024 * 1024 * num_threads * num_xstreams);
    /* Create Execution Streams */
    // ret = ABT_xstream_self(&xstreams[0]);
    for (i = 1; i < num_xstreams + 1; i++) {
      ret = ABT_xstream_create(ABT_SCHED_NULL, &xstreams[i]);
    }

    /* Get the pools attached to an execution stream */
    for (i = 1; i < num_xstreams + 1; i++) {
      ret = ABT_xstream_get_main_pools(xstreams[i], 1, pools + i);
    }
  }

  volatile bool stop_comm = false;

  std::thread comm_thread([&stop_comm] {
    while (!stop_comm) {
      MPI_Progress();
    }
  });

  int total_threads = num_xstreams * num_threads;
  ABT_thread thread[total_threads];

  for (SIZE = 1; SIZE <= 1 * 1024 * 1024; SIZE <<= 1) {
    if (rank == 0) {
      double times = 0;
      int time;
      for (time = 0; time < TOTAL + SKIP; time++) {
        MPI_Barrier(MPI_COMM_WORLD);
        /* Create threads */
        for (i = 0; i < total_threads; i++) {
          // size_t tid = i * num_threads + j + 1;
          ret = ABT_thread_create(pools[i % num_xstreams + 1], thread_func,
                                  (void*)(size_t)i, ABT_THREAD_ATTR_NULL,
                                  &thread[i]);
        }

        /* Switch to other user level threads */
        // ABT_thread_yield();

        double min = 2e9;
        double max = 0;
        /* Join Execution Streams */
        for (i = 0; i < total_threads; i++) {
          ABT_thread_join(thread[i]);
          min = std::min(start[i], min);
          max = std::max(end[i], max);
        }
        if (time >= SKIP) times += max - min;
      }
      printf("[%d] %f\n", SIZE, times * 1e6 / TOTAL / total_threads);
    } else {
      int time;
      void* buf = malloc(SIZE);
      for (time = 0; time < TOTAL + SKIP; time++) {
        MPI_Barrier(MPI_COMM_WORLD);
        for (i = 0; i < num_threads * num_xstreams; i++) {
          // size_t tid = i * num_threads + j + 1;
          MPI_Send(buf, SIZE, MPI_BYTE, 0, i, MPI_COMM_WORLD);
        }
      }
      free(buf);
      // printf("%f\n", (MPI_Wtime() - s) * 1e6 / TOTAL / num_xstreams /
      // num_threads);
    }
  }

  stop_comm = true;
  comm_thread.join();

  if (rank == 0) {
    /* Free Execution Streams */
    for (i = 1; i < num_xstreams; i++) {
      ret = ABT_xstream_free(&xstreams[i]);
    }
    free(alldata);
  }

  ABT_finalize();
  /* Finalize */
  free(pools);
  free(xstreams);
  MPI_Finalize();

  return ret;
}
