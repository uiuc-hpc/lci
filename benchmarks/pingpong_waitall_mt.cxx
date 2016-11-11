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

#include "mpiv.h"
#include <atomic>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <pthread.h>

#define MESSAGE_ALIGNMENT 64
#define MIN_MSG_SIZE 1
#define MAX_MSG_SIZE (1 << 22)
#define MYBUFSIZE (MAX_MSG_SIZE + MESSAGE_ALIGNMENT)
#define SKIP_LARGE 10
#define LOOP_LARGE 100
#define LARGE_MESSAGE_SIZE 8192

char* s_buf1;
char* r_buf1;
int skip = 1000;
int loop = 10000;
#define WIN 64

pthread_mutex_t finished_size_mutex;
pthread_cond_t finished_size_cond;

typedef struct thread_tag { int id; } thread_tag_t;

void send_thread(intptr_t arg);
void recv_thread(intptr_t arg);

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

std::atomic<int> f;

int numprocs, provided, myid, err;
static int THREADS = 1;
static int WORKERS = 1;

int main(int argc, char* argv[]) {
  MPIV_Init(&argc, &argv);
  if (argc > 2) {
    THREADS = atoi(argv[1]);
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

  MPIV_Start_worker(WORKERS);
  MPIV_Finalize();
}

static int size = 0;

void main_task(intptr_t) {
  int i = 0;
  r_buf1 = (char*)mpiv::malloc(MYBUFSIZE);
  s_buf1 = (char*)mpiv::malloc(MYBUFSIZE);
  thread* sr_threads = new thread[THREADS];
  thread_tag_t* tags = new thread_tag_t[THREADS];

  if (myid == 0) {
    fprintf(stdout, HEADER);
    fprintf(stdout, "%-*s%*s\n", 10, "# Size", FIELD_WIDTH, "Latency (us)");
    fflush(stdout);
    for (size = MIN_MSG_SIZE; size <= MAX_MSG_SIZE;
         size = (size ? size * 2 : 1)) {
      MPIV_Barrier(MPI_COMM_WORLD);
      for (i = 0; i < THREADS; i++) {
        sr_threads[i] =
            MPIV_spawn(i % WORKERS, send_thread, (intptr_t) i);
      }
      for (i = 0; i < THREADS; i++) {
        MPIV_join(sr_threads[i]);
      }
      MPIV_Barrier(MPI_COMM_WORLD);
    }
  } else {
    for (size = MIN_MSG_SIZE; size <= MAX_MSG_SIZE;
         size = (size ? size * 2 : 1)) {
      MPIV_Barrier(MPI_COMM_WORLD);
      double t_start = MPIV_Wtime();

      for (i = 0; i < THREADS; i++) {
        sr_threads[i] =
            MPIV_spawn(i % WORKERS, recv_thread, (intptr_t) i);
      }
      for (i = 0; i < THREADS; i++) {
        MPIV_join(sr_threads[i]);
      }
      double t_end = MPIV_Wtime();
      double t = t_end - t_start;
      printf("%d \t %.5f \n", size, WIN * (loop + skip) / t);
      MPIV_Barrier(MPI_COMM_WORLD);
    }
  }
  mpiv::free(r_buf1);
  mpiv::free(s_buf1);
}

void recv_thread(intptr_t arg) {
  int i, val, align_size;
  char *s_buf, *r_buf;
  val = (int) (arg); 

  align_size = MESSAGE_ALIGNMENT;

  s_buf = (char*)(((unsigned long)s_buf1 + (align_size - 1)) / align_size *
                  align_size);
  r_buf = (char*)(((unsigned long)r_buf1 + (align_size - 1)) / align_size *
                  align_size);

  if (size > LARGE_MESSAGE_SIZE) {
    loop = LOOP_LARGE;
    skip = SKIP_LARGE;
  }

  #if 0
  /* touch the data */
  for (i = 0; i < size; i++) {
    s_buf[i] = 'a';
    r_buf[i] = 'b';
  }
  #endif
  MPIV_Request req[WIN];
  for (i = val; i < (loop + skip); i += THREADS) {
      for (int k = 0; k < WIN; k++) {
          MPIV_Irecv(r_buf, size, MPI_CHAR, 0, i << 8 | k, MPI_COMM_WORLD, &req[k]);
      }
      MPIV_Waitall(WIN, req);
  }
}

extern double _tt;

void send_thread(intptr_t arg) {
  int i, align_size;
  char *s_buf, *r_buf;
  int val = (int) (arg); 
 
  double t_start = 0, t_end = 0, t = 0, latency;
  align_size = MESSAGE_ALIGNMENT;

  s_buf = (char*)(((unsigned long)s_buf1 + (align_size - 1)) / align_size *
                  align_size);
  r_buf = (char*)(((unsigned long)r_buf1 + (align_size - 1)) / align_size *
                  align_size);

  if (size > LARGE_MESSAGE_SIZE) {
    loop = LOOP_LARGE;
    skip = SKIP_LARGE;
  }

  #if 0
  /* touch the data */
  for (i = 0; i < size; i++) {
    s_buf[i] = 'a';
    r_buf[i] = 'b';
  }
  #endif
  MPIV_Request req[WIN];
  for (i = val; i < (loop + skip); i += THREADS) {
      for (int k = 0; k < WIN; k++) {
          MPIV_Send(s_buf, size, MPI_CHAR, 1, i << 8 | k, MPI_COMM_WORLD);
      }
  }
}

/* vi: set sw=4 sts=4 tw=80: */
