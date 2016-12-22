#define BENCHMARK "OSU MPIV Multi-threaded Latency Test"
/*
 * Copyright (C) 2002-2014 the Network-Based Computing Laboratory
 * (NBCL), The Ohio State University.
 *
 * Contact: Dr. D. K. Panda (panda@cse.ohio-state.edu)
 *
 * For detailed copyright and licensing information, please refer to the
 * copyright file COPYRIGHT in the top level OMB directory.
 */

#include "mv.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "comm_exp.h"

#ifdef USE_ABT
#include "mv/helper_abt.h"
#elif defined(USE_PTH)
#include "mv/helper_pth.h"
#else
#include "mv/helper.h"
#endif

#define MESSAGE_ALIGNMENT 64
#define MIN_MSG_SIZE 64
#define MAX_MSG_SIZE 64 //(1 << 22)
#define MYBUFSIZE (MAX_MSG_SIZE + MESSAGE_ALIGNMENT)
#define LARGE_MESSAGE_SIZE 8192

char* s_buf1;
char* r_buf1;
int skip = 0;
int loop = 1e5;

pthread_mutex_t finished_size_mutex;
pthread_cond_t finished_size_cond;

typedef struct thread_tag {
  int id;
} thread_tag_t;

void* send_thread(void* arg);
void* recv_thread(void* arg);

#ifdef PACKAGE_VERSION
#define HEADER "# " BENCHMARK " v" PACKAGE_VERSION "\n"
#else
#define HEADER "# " BENCHMARK "\n"
#endif

#ifndef FIELD_WIDTH
#define FIELD_WIDTH 20
#endif

#ifndef FLOAT_PRECISION
#define FLOAT_PRECISION 2
#endif

int numprocs, provided, myid, err;
static int NTHREADS = 1;
static int THREADS = 1;

static int WORKERS = 1;
static int MAX_WIN = 64;
static int WIN = 64;

int main(int argc, char* argv[])
{
  int provide;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provide);

  if (argc > 2) {
    NTHREADS = atoi(argv[1]);
    WORKERS = atoi(argv[2]);
  }

  MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
  MPI_Comm_rank(MPI_COMM_WORLD, &myid);

  if (numprocs != 2) {
    if (myid == 0) {
      fprintf(stderr, "This test requires exactly two processes\n");
    }

    MPIV_Finalize();
    return EXIT_FAILURE;
  }
  main_task(0);
  MPI_Finalize();
}

static int size = 0;

void main_task(intptr_t arg)
{
  set_me_to_(0);
  int i = 0;
  r_buf1 = (char*)malloc(MYBUFSIZE);
  s_buf1 = (char*)malloc(MYBUFSIZE);
  pthread_t* sr_threads = malloc(sizeof(pthread_t) * NTHREADS);
  thread_tag_t* tags = malloc(sizeof(thread_tag_t) * NTHREADS);

  if (myid == 0) {
    fprintf(stdout, HEADER);
    fprintf(stdout, "%-*s%*s\n", 10, "# THREAD", FIELD_WIDTH, "Msg rate (msg/s)");
    fflush(stdout);
    for (THREADS = 1 ; THREADS <= NTHREADS; THREADS *= 2)
    for (size = MIN_MSG_SIZE; size <= MAX_MSG_SIZE;
         size = (size ? size * 2 : 1)) {
      WIN = max(1, MAX_WIN / THREADS);
      MPI_Barrier(MPI_COMM_WORLD);
      for (int i = 0; i < THREADS; i++) {
        pthread_create(&sr_threads[i], NULL, send_thread, (void*) i);
      }
      for (int i = 0; i < THREADS; i++) {
        pthread_join(sr_threads[i], NULL);
      }
      MPI_Barrier(MPI_COMM_WORLD);
    }
  } else {
    for (THREADS = 1 ; THREADS <= NTHREADS; THREADS *= 2)
    for (size = MIN_MSG_SIZE; size <= MAX_MSG_SIZE;
         size = (size ? size * 2 : 1)) {
      WIN = max(1, MAX_WIN / THREADS);
      MPI_Barrier(MPI_COMM_WORLD);
      double t_start = MPI_Wtime();
      for (int i = 0; i < THREADS; i++) {
        pthread_create(&sr_threads[i], NULL, recv_thread, (void*) i);
      }
      for (int i = 0; i < THREADS; i++) {
        pthread_join(sr_threads[i], NULL);
      }
      MPI_Barrier(MPI_COMM_WORLD);
      double t = MPI_Wtime() - t_start;
      printf("%d %d \t %.5f \n", WIN, THREADS,  (loop + skip + (loop+skip)/WIN) / t);
    }
  }

  free(sr_threads);
  free(tags);
  free(r_buf1);
  free(s_buf1);
}

void* recv_thread(void* arg)
{
  int i, val, align_size;
  char *s_buf, *r_buf;
  val = (int)(arg);
  set_me_to_(val);

  align_size = MESSAGE_ALIGNMENT;

  s_buf = (char*)(((unsigned long)s_buf1 + (align_size - 1)) / align_size *
                  align_size);
  r_buf = (char*)(((unsigned long)r_buf1 + (align_size - 1)) / align_size *
                  align_size);

  if (size > LARGE_MESSAGE_SIZE) {
    loop = TOTAL_LARGE;
    skip = SKIP_LARGE;
  }

#if 0
  /* touch the data */
  for (i = 0; i < size; i++) {
    s_buf[i] = 'a';
    r_buf[i] = 'b';
  }
#endif
  MPI_Request req[WIN];
  for (i = val; i < (loop + skip) / WIN; i += THREADS) {
    for (int k = 0; k < WIN; k++) {
      MPI_Irecv(r_buf, size, MPI_CHAR, 0, val * WIN + k, MPI_COMM_WORLD, &req[k]);
      // MPIV_Recv(r_buf, size, MPI_CHAR, 0, i << 8 | k, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    MPI_Waitall(WIN, req, MPI_STATUSES_IGNORE);
    MPI_Ssend(s_buf, size, MPI_CHAR, 0, ((WIN+1) << 8) | val, MPI_COMM_WORLD);
  }
  return 0;
}

extern double _tt;

void* send_thread(void* arg)
{
  int i, align_size;
  char *s_buf, *r_buf;
  int val = (int)(arg);
  set_me_to_(val);

  double t_start = 0, t_end = 0, t = 0, latency;
  align_size = MESSAGE_ALIGNMENT;

  s_buf = (char*)(((unsigned long)s_buf1 + (align_size - 1)) / align_size *
                  align_size);
  r_buf = (char*)(((unsigned long)r_buf1 + (align_size - 1)) / align_size *
                  align_size);

  if (size > LARGE_MESSAGE_SIZE) {
    loop = TOTAL_LARGE;
    skip = SKIP_LARGE;
  }

#if 0
  /* touch the data */
  for (i = 0; i < size; i++) {
    s_buf[i] = 'a';
    r_buf[i] = 'b';
  }
#endif
  MPI_Request req[WIN];
  for (i = val; i < (loop + skip) / WIN; i += THREADS) {
    for (int k = 0; k < WIN; k++) {
      MPI_Isend(r_buf, size, MPI_CHAR, 1, val * WIN + k, MPI_COMM_WORLD, &req[k]);
    }
    MPI_Waitall(WIN, req, MPI_STATUSES_IGNORE);
    MPI_Recv(r_buf, size, MPI_CHAR, 1, ((WIN+1) << 8) | val, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  }
  return 0;
}

/* vi: set sw=4 sts=4 tw=80: */
