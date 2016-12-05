/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *   Copyright (C) 2007 University of Chicago
 *   See COPYRIGHT notice in top-level directory.
 */

#include "comm_exp.h"
#include "mpiv.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#define MAXSIZE (1 << 22)
#define MAX_THREADS 256

/* multithreaded version of latency.c */

/* two processes. each has NTHREADS threads including the main
 * thread. Each thread sends to (and receives from) corresponding thread
 * on other process many times. */

void runfunc(intptr_t);
int rank, nworkers, nprocs, i, nthreads, provided;
thread* id;

int main(int argc, char* argv[])
{
  MPIV_Init(&argc, &argv);

  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  if (nprocs != 2) {
    printf("Run with 2 processes\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  if (argc != 3) {
    printf("Error: a.out nthreads\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  nthreads = atoi(argv[1]);
  nworkers = atoi(argv[2]);

  printf("%d %d\n", nthreads, nworkers);

  MPIV_Init_worker(nworkers);

  MPIV_Finalize();
  return 0;
}

int size;
char *sendbuf, *recvbuf;
void main_task(intptr_t)
{
  sendbuf = (char*)mpiv_malloc(MAXSIZE);
  recvbuf = (char*)mpiv_malloc(MAXSIZE);
  int loop = std::max(TOTAL_LARGE, nthreads);
  printf("%d\n", loop);
  id = new thread[nthreads];

  for (size = 1; size <= MAXSIZE; size *= 2) {
    double t1 = wtime();
    MPIV_Barrier(MPI_COMM_WORLD);
    for (i = 0; i < nthreads; i++) {
      id[i] = MPIV_spawn(i % nworkers, runfunc, (intptr_t)i);
    }
    for (i = 0; i < nthreads; i++) MPIV_join(id[i]);
    t1 = wtime() - t1;
    MPIV_Barrier(MPI_COMM_WORLD);
    if (rank == 0) printf("[%d] \t %.2f\n", size, 1e6 * t1 / 2 / loop);
  }
  mpiv_free(sendbuf);
  mpiv_free(recvbuf);
}

void runfunc(intptr_t thread_rank)
{
  int src, dest, tag, i;
  /* All even ranks send to (and recv from) rank i+1 many times */
  tag = (int)thread_rank;
  int loop = std::max(TOTAL_LARGE, nthreads);
  if ((rank % 2) == 0) { /* even */
    memset(recvbuf, 'a', size);
    memset(sendbuf, 'b', size);
    dest = rank + 1;
    for (i = tag; i < loop; i += nthreads) {
      MPIV_Send(sendbuf, size, MPI_BYTE, dest, tag, MPI_COMM_WORLD);
      MPIV_Recv(recvbuf, size, MPI_BYTE, dest, tag, MPI_COMM_WORLD,
                MPI_STATUS_IGNORE);
    }
  } else { /* odd */
    memset(sendbuf, 'a', size);
    memset(recvbuf, 'b', size);
    src = rank - 1;
    for (i = tag; i < loop; i += nthreads) {
      MPIV_Recv(recvbuf, size, MPI_BYTE, src, tag, MPI_COMM_WORLD,
                MPI_STATUS_IGNORE);
      MPIV_Send(sendbuf, size, MPI_BYTE, src, tag, MPI_COMM_WORLD);
    }
  }
}
