/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *   Copyright (C) 2007 University of Chicago
 *   See COPYRIGHT notice in top-level directory.
 */

// #define USE_L1_MASK

#include "mv.h"
#include "comm_exp.h"

#ifdef USE_ABT
#include "mv/helper_abt.h"
#elif defined(USE_PTH)
#include "mv/helper_pth.h"
#else
#include "mv/helper.h"
#endif

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <math.h>

#define MINSIZE (64)
#define MAXSIZE (64)
#define TOTAL_MSG (1e5)

/* multithreaded version of latency.c */

/* two processes. each has NTHREADS threads including the main
 * thread. Each thread sends to (and receives from) corresponding thread
 * on other process many times. */

void runfunc(intptr_t);
void print_func(intptr_t);

int rank, nworkers, nprocs, i, min_nthreads, max_nthreads, provided;
int WORK;
mv_thread* id;

// #define ARRAY_SIZE 1024 * 1024 * 1024
// static char trash[ARRAY_SIZE];

static inline uint64_t rdtsc()
{
  unsigned int lo, hi;
  __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
  return ((uint64_t)hi << 32) | lo;
}

static long find_prime(int n) {
  int count=0;
  long a = 2;
  while(count<n) {
    long b = 2;
    int prime = 1;
    while(b * b <= a) {
      if(a % b == 0)
      {
        prime = 0;
        break;
      }
      b++;
    }
    if(prime > 0)
      count++;
    a++;
  }
  return (--a);
}

// __attribute__((noinline))
static int compute(uint64_t size) {
  uint64_t start = rdtsc();
  while (rdtsc() - start < size) {
    ;
  }
}

int main(int argc, char* argv[])
{
  MPIV_Init(&argc, &argv);

  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  if (nprocs != 2) {
    printf("Run with 2 processes\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  if (argc != 5) {
    printf("Error: a.out min_nthreads max_nthreads n_workers\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  min_nthreads = atoi(argv[1]);
  max_nthreads = atoi(argv[2]);
  nworkers = atoi(argv[3]);
  WORK = atoi(argv[4]);

  MPIV_Start_worker(nworkers, 0);
  MPIV_Finalize();
  return 0;
}

int size, work, comm;
char *sendbuf, *recvbuf;
int nthreads;

int* cache_buf;
int cache_size;

static void cache_invalidate(void)
{
  int i;
  cache_buf[0] = 1;
  for (i = 1; i < cache_size; ++i) {
    cache_buf[i] = cache_buf[i - 1];
  }
}

__thread double tcomp = 0;

void main_task(intptr_t a)
{
  sendbuf = (char*)MPIV_Alloc(MAXSIZE * max_nthreads);
  recvbuf = (char*)MPIV_Alloc(MAXSIZE * max_nthreads);
  id = malloc(sizeof(mv_thread) * max_nthreads);
  cache_size = (8 * 1024 * 1024 / sizeof(int));
  cache_buf = (int*)malloc(sizeof(int) * cache_size);

  for (nthreads = min_nthreads; nthreads <= max_nthreads; nthreads *= 2)
    for (work = WORK; work <= 8192 * 1000; work *= 2) {
      cache_invalidate();
      int loop = TOTAL_MSG/nthreads*nthreads;
      double t3 = wtime();
      for (int i = 0; i < loop; i++)
        compute(work);
      t3 = (wtime() - t3) / loop;
      tcomp = 0;

      for (size = MINSIZE; size <= MAXSIZE; size *= 2) {
        MPI_Barrier(MPI_COMM_WORLD);
        double t1 = wtime();
        for (i = 0; i < nthreads; i++)
          id[i] = MPIV_spawn(i % nworkers, runfunc, (intptr_t)i);
        for (i = 0; i < nthreads; i++)
          MPIV_join(id[i]);

        MPI_Barrier(MPI_COMM_WORLD);
        t1 = wtime() - t1;
        if (rank == 1) printf("%d \t %d \t %.2f \t %.2f\n", work, nthreads, (double) loop * 2 / t1, tcomp / t1 * 100);
      }
    }
  free(id);
}

#if 0
void print_func(intptr_t thread_rank)
{
  if (rank == 0) {
    wtimework += wtime();
    wtimeall += wtime();
    double wtimeidle = wtimeall - wtimework;
    printf("%d, %.5f, %.5f, %.5f, %.5f\n", thread_rank, wtimeall, wtimeidle, tcomp, tcomp / wtimeall * 100);
  }
}
#endif

void runfunc(intptr_t thread_rank)
{
  int src, dest, tag, i;
  /* All even ranks send to (and recv from) rank i+1 many times */
  tag = (int)thread_rank;
  int loop = TOTAL_MSG/nthreads*nthreads; //max(TOTAL, nthreads * 100);
  // loop = nthreads;

  void* lsendbuf = ((char*) sendbuf + thread_rank * 64);
  void* lrecvbuf = ((char*) recvbuf + thread_rank * 64);

  if ((rank % 2) == 0) { /* even */
    // memset(recvbuf, 'a', size);
    // memset(sendbuf, 'b', size);
    dest = rank + 1;
    for (i = tag; i < loop; i += nthreads) {
      MPIV_Recv(lrecvbuf, size, MPI_BYTE, dest, i, MPI_COMM_WORLD,
          MPI_STATUS_IGNORE);
      tcomp-=wtime();
      compute(work);
      tcomp+=wtime();
      MPIV_Send(lsendbuf, size, MPI_BYTE, dest, i, MPI_COMM_WORLD);
      tcomp-=wtime();
      compute(work);
      tcomp+=wtime();
    }
  } else { /* odd */
    // memset(sendbuf, 'a', size);
    // memset(recvbuf, 'b', size);
    src = rank - 1;
    for (i = tag; i < loop; i += nthreads) {
      MPIV_Send(lsendbuf, size, MPI_BYTE, src, i, MPI_COMM_WORLD);
      tcomp-=wtime();
      compute(work);
      tcomp+=wtime();
      MPIV_Recv(lrecvbuf, size, MPI_BYTE, src, i, MPI_COMM_WORLD,
          MPI_STATUS_IGNORE);
      tcomp-=wtime();
      compute(work);
      tcomp+=wtime();
    }
  }

  // if (rank == 0 && tag == 0)
    // printf("%.5f\n", work);
}
