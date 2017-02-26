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
#include "mv/helper.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MESSAGE_ALIGNMENT 64
#define MIN_MSG_SIZE 1
#define MAX_MSG_SIZE (1 << 22)
#define MYBUFSIZE (MAX_MSG_SIZE + MESSAGE_ALIGNMENT)
#define SKIP_LARGE 100
#define LOOP_LARGE 1000
#define LARGE_MESSAGE_SIZE 8192

char* s_buf1;
char* r_buf1;
int skip = 1000;
int loop = 10000;

pthread_mutex_t finished_size_mutex;
pthread_cond_t finished_size_cond;

typedef struct thread_tag {
  int id;
} thread_tag_t;

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

int numprocs, provided, myid, err;
static int THREADS = 1;
static int WORKERS = 1;
static int WINDOWS = 1;

int main(int argc, char* argv[])
{
  MPIV_Init(&argc, &argv);
  if (argc > 2) {
    THREADS = atoi(argv[1]);
    WORKERS = atoi(argv[2]);
    WINDOWS = atoi(argv[3]);
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

  MPIV_Start_worker(WORKERS, 0);

  MPIV_Finalize();
}

static int size = 0;

void main_task(intptr_t arg)
{
  int i = 0;
  r_buf1 = (char*)malloc(MYBUFSIZE);
  s_buf1 = (char*)malloc(MYBUFSIZE);
  mv_thread sr_threads[THREADS];
  thread_tag_t tags[THREADS];

  if (myid == 0) {
    fprintf(stdout, HEADER);
    fprintf(stdout, "%-*s%*s\n", 10, "# Size", FIELD_WIDTH, "Latency (us)");
    fflush(stdout);
    for (size = MIN_MSG_SIZE; size <= MAX_MSG_SIZE;
         size = (size ? size * 2 : 1)) {
      MPI_Barrier(MPI_COMM_WORLD);
      int i = 0;
      // for (i = 0; i < THREADS; i++)
          sr_threads[i] = MPIV_spawn(i % WORKERS, send_thread, (intptr_t)i);
      // for (i = 0; i < THREADS; i++)
          MPIV_join(sr_threads[i]);
      MPI_Barrier(MPI_COMM_WORLD);
    }
  } else {
    for (size = MIN_MSG_SIZE; size <= MAX_MSG_SIZE;
         size = (size ? size * 2 : 1)) {
      MPI_Barrier(MPI_COMM_WORLD);
      // printf("r spawn\n");
      for (i = 0; i < THREADS; i++) {
        sr_threads[i] = MPIV_spawn(i % WORKERS, recv_thread, (intptr_t)i);
      }
      for (i = 0; i < THREADS; i++) {
        MPIV_join(sr_threads[i]);
      }
      // printf("r join\n");
      MPI_Barrier(MPI_COMM_WORLD);
    }
  }
  free(r_buf1);
  free(s_buf1);
}

void recv_thread(intptr_t arg)
{
  int i, val, align_size;
  char *s_buf, *r_buf;
  val = (int)(arg);

  align_size = MESSAGE_ALIGNMENT;

  s_buf = (char*)(((unsigned long)s_buf1 + (align_size - 1)) / align_size *
                  align_size);
  r_buf = (char*)(((unsigned long)r_buf1 + (align_size - 1)) / align_size *
                  align_size);

  if (size > LARGE_MESSAGE_SIZE) {
    loop = LOOP_LARGE;
    skip = SKIP_LARGE;
  }

  /* touch the data */
  for (i = 0; i < size; i++) {
    s_buf[i] = 'a';
    r_buf[i] = 'b';
  }

   mv_ctx ctxs;
   void* buf;
   int len, rank, tag;

  for (i = val; i < loop + skip; i+=THREADS) {
    // recv
    while (!mv_recv_dequeue_init(mv_hdl, &len, &rank, &tag, &ctxs))
          ;
    void* b = malloc(len);
    mv_recv_dequeue_post(mv_hdl, b, &ctxs);
    free(b);

    while (!mv_test(&ctxs))
        ;

    while (!mv_send_enqueue_init(mv_hdl, s_buf, size, 0, 0, &ctxs))
        ;
  }
  sleep(0.5);
}

void send_thread(intptr_t arg)
{
  int i, align_size;
  char *s_buf, *r_buf;
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

  /* touch the data */
  for (i = 0; i < size; i++) {
    s_buf[i] = 'a';
    r_buf[i] = 'b';
  }

  mv_ctx ctxr;
  void* buf;
  int len, rank, tag;

  for (i = 0; i < loop + skip; i++) {
    if (i == skip) {
      t_start = MPI_Wtime();
    }
    // send
    //for (int j = 0; j < WINDOWS; j++)
    while (!mv_send_enqueue_init(mv_hdl, s_buf, size, 1, 0, &ctxr))
        ;

    // recv.
    // for (int j = 0; j < WINDOWS; j++) {
    while (!mv_recv_dequeue_init(mv_hdl, &len, &rank, &tag, &ctxr))
        ;
    void* buf = malloc(len);
    mv_recv_dequeue_post(mv_hdl, buf, &ctxr);
    while (!mv_test(&ctxr))
        ;
    free(buf);
  }

  t_end = MPI_Wtime();
  t = t_end - t_start;

  latency = (t)*1.0e6 / (2.0 * loop);
  printf("[%d] %.3f\n", size, latency);
}

/* vi: set sw=4 sts=4 tw=80: */
