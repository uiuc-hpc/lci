/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *   Copyright (C) 2007 University of Chicago
 *   See COPYRIGHT notice in top-level directory.
 */

#include "mv.h"
#include "comm_exp.h"

#define USE_L1_MASK

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
#define MAX_THREADS 256

/* multithreaded version of latency.c */

/* two processes. each has NTHREADS threads including the main
 * thread. Each thread sends to (and receives from) corresponding thread
 * on other process many times. */

void* runfunc(void*);
int rank, nworkers, nprocs, i, min_nthreads, max_nthreads, provided;
pthread_t* id;

int main(int argc, char* argv[])
{
  // MPIV_Init(&argc, &argv);
  int provide;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provide);

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

  // MPIV_Start_worker(nworkers, 0);
  main_task(0);
  MPI_Finalize();
  return 0;
}

int size;
char *sendbuf, *recvbuf;
int nthreads;
int loop = 1e5;

void main_task(intptr_t a)
{
  set_me_to_(0);
  sendbuf = (char*)malloc(MAXSIZE);
  recvbuf = (char*)malloc(MAXSIZE);
  id = malloc(sizeof(pthread_t) * max_nthreads);

  for (nthreads = min_nthreads; nthreads <= max_nthreads; nthreads *= 2)
    for (size = MINSIZE; size <= MAXSIZE; size *= 2) {
      double t1 = MPI_Wtime();
      MPI_Barrier(MPI_COMM_WORLD);
      for (i = 0; i < nthreads; i++) {
        pthread_create(&id[i], NULL, runfunc, (void*)i);
      }
      for (i = 0; i < nthreads; i++) {
        pthread_join(id[i], 0);
      }
      t1 = MPI_Wtime() - t1;
      MPI_Barrier(MPI_COMM_WORLD);
      if (rank == 0)
        printf("%d \t %d \t %.2f\n", size, nthreads, (2 * loop) / t1);
    }
  free(id);
  free(sendbuf);
  free(recvbuf);
}

void* runfunc(void* arg)
{
  intptr_t thread_rank = (intptr_t)arg;
  set_me_to_(thread_rank);

  int src, dest, tag, i;
  /* All even ranks send to (and recv from) rank i+1 many times */
  tag = (int)thread_rank;

  if ((rank % 2) == 0) { /* even */
    memset(recvbuf, 'a', size);
    memset(sendbuf, 'b', size);
    dest = rank + 1;
    for (i = tag; i < loop; i += nthreads) {
      MPI_Send(sendbuf, size, MPI_BYTE, dest, tag, MPI_COMM_WORLD);
      MPI_Recv(recvbuf, size, MPI_BYTE, dest, tag, MPI_COMM_WORLD,
               MPI_STATUS_IGNORE);
    }
  } else { /* odd */
    memset(sendbuf, 'a', size);
    memset(recvbuf, 'b', size);
    src = rank - 1;
    for (i = tag; i < loop; i += nthreads) {
      MPI_Recv(recvbuf, size, MPI_BYTE, src, tag, MPI_COMM_WORLD,
               MPI_STATUS_IGNORE);
      MPI_Send(sendbuf, size, MPI_BYTE, src, tag, MPI_COMM_WORLD);
    }
  }
  return 0;
}
