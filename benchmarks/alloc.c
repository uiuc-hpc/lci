#include <assert.h>
#include <mpi.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

// #define CHECK_RESULT

#include "comm_exp.h"
#include "helper.h"

#include "profiler.h"

#define CHECK_RESULT 0

#if 0
#undef total
#define total 20
#undef skip
#define skip 0
#endif

#define MIN_MSG_SIZE 1
#define MAX_MSG_SIZE (1 << 22)

int size = 0;

int main(int argc, char** args)
{
  MPIV_Init(&argc, &args);
  if (argc > 1) size = atoi(args[1]);
  MPIV_Start_worker(1);
  MPIV_Finalize();
  return 0;
}

void main_task(intptr_t arg)
{
  double times = 0;
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  void* r_buf = (void*) MPIV_Alloc((size_t)MAX_MSG_SIZE);
  void* s_buf = (void*) MPIV_Alloc((size_t)MAX_MSG_SIZE);
  memset(r_buf, 'a', size);
  memset(s_buf, 'b', size);
  MPIV_Free(r_buf);
  MPIV_Free(s_buf);
}
