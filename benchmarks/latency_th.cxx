/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *   Copyright (C) 2007 University of Chicago
 *   See COPYRIGHT notice in top-level directory.
 */

#include "mpi.h"
#include "affinity.h"
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <strings.h>

#define MAXSIZE (1<<22)
#define NTIMES 1000
#define MAX_THREADS 12

/* multithreaded version of latency.c */

/* two processes. each has NTHREADS threads including the main
 * thread. Each thread sends to (and receives from) corresponding thread 
 * on other process many times. */

void *runfunc(void *);

int nworkers;

int main(int argc,char *argv[])
{
  int rank, nprocs, i, nthreads, provided;
  pthread_t id[MAX_THREADS];
  int thread_ranks[MAX_THREADS];

  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
  if (provided != MPI_THREAD_MULTIPLE) {
    printf("Thread multiple needed\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

  MPI_Comm_size(MPI_COMM_WORLD,&nprocs);
  if (nprocs != 2) {
    printf("Run with 2 processes\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);

  if (!rank) {
    if (argc != 3) {
      printf("Error: a.out nthreads\n");
      MPI_Abort(MPI_COMM_WORLD, 1);
    }

    nthreads = atoi(argv[1]);
    nworkers = atoi(argv[2]);
    MPI_Send(&nthreads, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
  }
  else
    MPI_Recv(&nthreads, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);


  for (i=0; i<nthreads; i++) {
    thread_ranks[i] = i;
    pthread_create(&id[i], NULL, runfunc, (void *) &thread_ranks[i]);
  }

  for (i=0; i<nthreads; i++)
    pthread_join(id[i], NULL);

  MPI_Finalize();
  return 0;
}


void *runfunc(void *thread_rank) {
  affinity::set_me_within(0, nworkers);
  int rank, src, dest, tag, i, size, incr;
  double stime, etime, ttime;
  char *sendbuf, *recvbuf;

  MPI_Comm_rank(MPI_COMM_WORLD,&rank);

  sendbuf = (char *) malloc(MAXSIZE);
  if (!sendbuf) {
    printf("Cannot allocate buffer\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

  recvbuf = (char *) malloc(MAXSIZE);
  if (!recvbuf) {
    printf("Cannot allocate buffer\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

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
        MPI_Send(sendbuf, size, MPI_BYTE, dest, tag, MPI_COMM_WORLD);
        MPI_Recv(recvbuf, size, MPI_BYTE, dest, tag, MPI_COMM_WORLD, 
            MPI_STATUS_IGNORE);
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
        MPI_Recv(recvbuf, size, MPI_BYTE, src, tag, MPI_COMM_WORLD, 
            MPI_STATUS_IGNORE);    
        MPI_Send(sendbuf, size, MPI_BYTE, src, tag, MPI_COMM_WORLD);
      }
      if (size == 256) incr = 64;
    }
  }

  free(sendbuf);
  free(recvbuf);
  pthread_exit(NULL);
  return 0;
}
