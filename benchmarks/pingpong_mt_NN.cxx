/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *   Copyright (C) 2007 University of Chicago
 *   See COPYRIGHT notice in top-level directory.
 */

#include "mpiv.h"
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <strings.h>

#define MAXSIZE (1 << 22)
#define NTIMES 1000
#define MAX_THREADS 16

/* multithreaded version of latency.c */

/* two processes. each has NTHREADS threads including the main
 * thread. Each thread sends to (and receives from) corresponding thread 
 * on other process many times. */

void runfunc(intptr_t);
int rank, nworkers, nprocs, i, nthreads, provided;
fult_t id[MAX_THREADS];
int thread_ranks[MAX_THREADS];

int main(int argc,char *argv[])
{
  MPIV_Init(argc, argv);

  MPI_Comm_size(MPI_COMM_WORLD,&nprocs);
  if (nprocs != 2) {
    printf("Run with 2 processes\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);

  if (argc != 3) {
    printf("Error: a.out nthreads\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  nthreads = atoi(argv[1]);
  nworkers = atoi(argv[2]);

  MPIV_Init_worker(nworkers);

  MPIV_Finalize();
  return 0;
}

void main_task(intptr_t) {
  for (i=0; i<nthreads; i++) {
    thread_ranks[i] = i;
    id[i] = MPIV_spawn(i % nworkers, runfunc, (intptr_t) &thread_ranks[i]);
  }

  for (i=0; i<nthreads; i++)
    MPIV_join(i % nworkers, id[i]);
}


void runfunc(intptr_t thread_rank) {
  int src, dest, tag, i, size, incr;
  double stime, etime, ttime;
  char *sendbuf, *recvbuf;

  sendbuf = (char *) mpiv_malloc(MAXSIZE);
  recvbuf = (char *) mpiv_malloc(MAXSIZE);

  /* All even ranks send to (and recv from) rank i+1 many times */
  incr = 16;
  tag = * (int *)thread_rank;
  if ((rank % 2) == 0) { /* even */
    dest = rank + 1;

    if ((* (int *)thread_rank) == 0)
      printf("Size (bytes) \t Time (us)\n");

    for (size=1; size<=MAXSIZE; size*=2) {
      stime = MPI_Wtime();
      for (i=0; i<NTIMES; i++) {
        MPIV_Send(sendbuf, size, MPI_BYTE, dest, tag, MPI_COMM_WORLD);
        MPIV_Recv(recvbuf, size, MPI_BYTE, dest, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      }
      etime = MPI_Wtime();

      ttime = (etime - stime)/(2*NTIMES);

      if ((* (int *)thread_rank) == 0)
        printf("%d \t %f\n", size, ttime*1000000);

      if (size == 256) incr = 64;
    }
  }
  else {  /* odd */
    src = rank - 1;

    for (size=1; size<=MAXSIZE; size*=2) {
      for (i=0; i<NTIMES; i++) {
        MPIV_Recv(recvbuf, size, MPI_BYTE, src, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);    
        MPIV_Send(sendbuf, size, MPI_BYTE, src, tag, MPI_COMM_WORLD);
      }
      if (size == 256) incr = 64;
    }
  }

  mpiv_free(sendbuf);
  mpiv_free(recvbuf);
}
