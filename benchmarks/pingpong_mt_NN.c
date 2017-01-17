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

#define MINSIZE (64)
#define MAXSIZE (64)
#define TOTAL_MSG 1e6

/* multithreaded version of latency.c */

/* two processes. each has NTHREADS threads including the main
 * thread. Each thread sends to (and receives from) corresponding thread
 * on other process many times. */

void runfunc(intptr_t);
int rank, nworkers, nprocs, i, min_nthreads, max_nthreads, provided;
mv_thread* id;

int main(int argc, char* argv[])
{
  MPIV_Init(&argc, &argv);

  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  if (nprocs != 2) {
    printf("Run with 2 processes\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  if (argc != 4) {
    printf("Error: a.out min_nthreads max_nthreads n_workers\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  min_nthreads = atoi(argv[1]);
  max_nthreads = atoi(argv[2]);
  nworkers = atoi(argv[3]);

  MPIV_Start_worker(nworkers, 0);
  MPIV_Finalize();
  return 0;
}

int size;
char *sendbuf, *recvbuf;
int nthreads;

void main_task(intptr_t a)
{
  sendbuf = (char*)MPIV_Alloc(MAXSIZE * max_nthreads);
  recvbuf = (char*)MPIV_Alloc(MAXSIZE * max_nthreads);
  id = malloc(sizeof(mv_thread) * max_nthreads);

  for (nthreads = min_nthreads; nthreads <= max_nthreads; nthreads *= 2)
    for (size = MINSIZE; size <= MAXSIZE; size *= 2) {
      int loop = TOTAL_MSG;
      MPI_Barrier(MPI_COMM_WORLD);
      double t1 = wtime();
      for (i = 0; i < nthreads; i++) {
        id[i] = MPIV_spawn(i % nworkers, runfunc, (intptr_t)i);
      }
      for (i = 0; i < nthreads; i++) MPIV_join(id[i]);
      MPI_Barrier(MPI_COMM_WORLD);
      t1 = wtime() - t1;
      if (rank == 0) printf("%d \t %d \t %.2f\n", size, nthreads, (2 * loop) / t1);
    }
  free(id);
}

void runfunc(intptr_t thread_rank)
{
  int src, dest, tag, i;
  /* All even ranks send to (and recv from) rank i+1 many times */
  tag = (int)thread_rank;
  int loop = TOTAL_MSG; //max(TOTAL, nthreads * 100);

  void* lsendbuf = ((char*) sendbuf + thread_rank * 64);
  void* lrecvbuf = ((char*) recvbuf + thread_rank * 64);

  if ((rank % 2) == 0) { /* even */
    // memset(recvbuf, 'a', size);
    // memset(sendbuf, 'b', size);
    dest = rank + 1;
    for (i = tag; i < loop; i += nthreads) {
      MPIV_Send(lsendbuf, size, MPI_BYTE, dest, tag, MPI_COMM_WORLD);
      MPIV_Recv(lrecvbuf, size, MPI_BYTE, dest, tag, MPI_COMM_WORLD,
                MPI_STATUS_IGNORE);
    }
  } else { /* odd */
    // memset(sendbuf, 'a', size);
    // memset(recvbuf, 'b', size);
    src = rank - 1;
    for (i = tag; i < loop; i += nthreads) {
      MPIV_Recv(lrecvbuf, size, MPI_BYTE, src, tag, MPI_COMM_WORLD,
                MPI_STATUS_IGNORE);
      MPIV_Send(lsendbuf, size, MPI_BYTE, src, tag, MPI_COMM_WORLD);
    }
  }
}
