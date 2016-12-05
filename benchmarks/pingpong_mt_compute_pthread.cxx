#define BENCHMARK "OSU MPI Multi-threaded Latency Test"
/*
 * Copyright (C) 2002-2014 the Network-Based Computing Laboratory
 * (NBCL), The Ohio State University.
 *
 * Contact: Dr. D. K. Panda (panda@cse.ohio-state.edu)
 *
 * For detailed copyright and licensing information, please refer to the
 * copyright file COPYRIGHT in the top level OMB directory.
 */

#include "affinity.h"
#include <atomic>
#include <cstdlib>
#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MESSAGE_ALIGNMENT 64
#define MAX_MSG_SIZE (1 << 20)
#define MYBUFSIZE (MAX_MSG_SIZE + MESSAGE_ALIGNMENT)
#define SKIP_LARGE 100
#define LOOP_LARGE 1000
#define LARGE_MESSAGE_SIZE 8192

#define BARRIER(f, n)                            \
  {                                              \
    int x = f.fetch_add(1);                      \
    if (x == n - 1) MPI_Barrier(MPI_COMM_WORLD); \
    while (f < n) {                              \
    }                                            \
  }

char* s_buf;
char* r_buf;
int skip = 1000;
int loop = 10000;

pthread_mutex_t finished_size_mutex;
pthread_cond_t finished_size_cond;

int finished_size;

static int size = 0;

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

static int THREADS = 1;
static int WORKERS = 1;
std::atomic<int> f;

int main(int argc, char* argv[])
{
  if (argc > 2) {
    THREADS = atoi(argv[1]);
    WORKERS = atoi(argv[2]);
  }

  int numprocs, provided, myid, err;
  int i = 0;
  pthread_t sr_threads[THREADS];
  thread_tag_t tags[THREADS];

  pthread_mutex_init(&finished_size_mutex, NULL);
  pthread_cond_init(&finished_size_cond, NULL);

  err = MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

  if (err != MPI_SUCCESS) {
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

  MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
  MPI_Comm_rank(MPI_COMM_WORLD, &myid);

  srand48(1238);

  if (numprocs != 2) {
    if (myid == 0) {
      fprintf(stderr, "This test requires exactly two processes\n");
    }

    MPI_Finalize();

    return EXIT_FAILURE;
  }

  /* Check to make sure we actually have a thread-safe
   * implementation
   */

  finished_size = 1;

  if (provided != MPI_THREAD_MULTIPLE) {
    if (myid == 0) {
      fprintf(stderr, "MPI_Init_thread must return MPI_THREAD_MULTIPLE!\n");
    }

    MPI_Finalize();

    return EXIT_FAILURE;
  }

  int align_size = MESSAGE_ALIGNMENT;

  if (posix_memalign((void**)&s_buf, align_size, MYBUFSIZE)) {
    fprintf(stderr, "Error allocating host memory\n");
  }

  if (posix_memalign((void**)&r_buf, align_size, MYBUFSIZE)) {
    fprintf(stderr, "Error allocating host memory\n");
  }

  if (myid == 0) {
    fprintf(stdout, HEADER);
    fprintf(stdout, "%-*s%*s\n", 10, "# Size", FIELD_WIDTH, "Latency (us)");
    fflush(stdout);

    for (size = 1; size <= MAX_MSG_SIZE; size = (size ? size * 2 : 1)) {
      send_thread(&tags[i]);
    }
  }

  else {
    for (size = 1; size <= MAX_MSG_SIZE; size = (size ? size * 2 : 1)) {
      for (i = 0; i < THREADS; i++) {
        tags[i].id = i;
        recv_thread((void*)(long)i);
      }
    }
  }

  MPI_Finalize();

  return EXIT_SUCCESS;
}

#define ARRAY_SIZE (64 * 1024 * 1024)
static char trash[ARRAY_SIZE];

void* recv_thread(void* arg)
{
  int i, val;

  val = (int)(long)arg;
  // affinity::set_me_to(val % WORKERS);

  if (size > LARGE_MESSAGE_SIZE) {
    loop = LOOP_LARGE;
    skip = SKIP_LARGE;
  }

  /* touch the data */
  for (i = 0; i < size; i++) {
    s_buf[i] = 'a';
    r_buf[i] = 'b';
  }

  double t_start = 0, t_end = 0, t = 0, latency;
  MPI_Barrier(MPI_COMM_WORLD);

  for (i = val; i < (loop + skip); i += THREADS) {
    MPI_Recv(r_buf, 64, MPI_CHAR, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    int loop = lrand48() % size;
    for (int ii = 0; ii < loop; ii++) {
      trash[lrand48() % ARRAY_SIZE] += r_buf[lrand48() % 64];
    }

    MPI_Send(s_buf, 64, MPI_CHAR, 0, 1, MPI_COMM_WORLD);
  }

  return 0;
}

void* send_thread(void*)
{
  // affinity::set_me_to(0);

  int i;
  double t_start = 0, t_end = 0, t = 0, latency;

  if (size > LARGE_MESSAGE_SIZE) {
    loop = LOOP_LARGE;
    skip = SKIP_LARGE;
  }

  /* touch the data */
  for (i = 0; i < size; i++) {
    s_buf[i] = 'a';
    r_buf[i] = 'b';
  }

  MPI_Barrier(MPI_COMM_WORLD);

  for (i = 0; i < loop + skip; i++) {
    if (i == skip) {
      t_start = MPI_Wtime();
    }

    MPI_Send(s_buf, 64, MPI_CHAR, 1, 1, MPI_COMM_WORLD);
    MPI_Recv(r_buf, 64, MPI_CHAR, 1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  }

  t_end = MPI_Wtime();
  t = t_end - t_start;

  latency = (t)*1.0e6 / (2.0 * loop);
  fprintf(stdout, "%-*d%*.*f\n", 10, size, FIELD_WIDTH, FLOAT_PRECISION,
          latency);
  fflush(stdout);

  return 0;
}

/* vi: set sw=4 sts=4 tw=80: */
