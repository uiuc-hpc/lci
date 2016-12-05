#include <assert.h>
#include <atomic>
#include <mpi.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <thread>
#include <unistd.h>

// #define CHECK_RESULT

#include "comm_exp.h"
#include "mpiv.h"

#include "profiler.h"

#define FIELD_WIDTH 18
#define FLOAT_PRECISION 2

#if 0
#undef total
#define total 20
#undef skip
#define skip 0
#endif

#define MIN_MSG_SIZE 1
#define MAX_MSG_SIZE (1 << 20)

int size = 0;

int main(int argc, char** args)
{
  MPIV_Init(&argc, &args);
  if (argc > 1) size = atoi(args[1]);
  MPIV_Start_worker(1);
  MPIV_Finalize();
  return 0;
}

#define ARRAY_SIZE 1024 * 1024 * 1024
static char trash[ARRAY_SIZE];

uint64_t rdtsc()
{
  unsigned int lo, hi;
  __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
  return ((uint64_t)hi << 32) | lo;
}

void* r_buf;

void compute(int size)
{
#if 1
  for (int ii = 0; ii < size; ii++) {
    trash[lrand48() % ARRAY_SIZE] += ((char*)r_buf)[lrand48() % 64];
  }
#else
  uint64_t start = rdtsc();
  while (rdtsc() - start < 10 * size)
    ;
#endif
}

void main_task(intptr_t)
{
  double times = 0;
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  r_buf = (void*)mv_malloc((size_t)MAX_MSG_SIZE);
  void* s_buf = (void*)mv_malloc((size_t)MAX_MSG_SIZE);
  srand48(1238);

  for (int size = MIN_MSG_SIZE; size <= MAX_MSG_SIZE; size <<= 1) {
    int total = TOTAL;
    int skip = SKIP;

    if (size > LARGE) {
      total = TOTAL_LARGE;
      skip = SKIP_LARGE;
    }
    MPI_Barrier(MPI_COMM_WORLD);
    times = 0;
    if (rank == 0) {
      memset(r_buf, 'a', size);
      memset(s_buf, 'b', size);
      for (int t = 0; t < total + skip; t++) {
        if (t == skip) {
          times = MPI_Wtime();
        }

        MPIV_Send(s_buf, 64, MPI_CHAR, 1, 1, MPI_COMM_WORLD);
        MPIV_Recv(r_buf, 64, MPI_CHAR, 1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        compute(size);
      }
      times = MPI_Wtime() - times;
    } else {
      memset(s_buf, 'b', size);
      memset(r_buf, 'a', size);
      for (int t = 0; t < total + skip; t++) {
        MPIV_Recv(r_buf, 64, MPI_CHAR, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        compute(size);
        MPIV_Send(s_buf, 64, MPI_CHAR, 0, 1, MPI_COMM_WORLD);
      }
    }
    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == 0) {
      double latency = times * 1e6 / (2.0 * total);
      fprintf(stdout, "%-*d%*.*f\n", 10, size, FIELD_WIDTH, FLOAT_PRECISION,
              latency);
      fflush(stdout);
    }
  }

  mv_free(r_buf);
  mv_free(s_buf);
}
