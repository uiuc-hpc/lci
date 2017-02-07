#include <assert.h>
#include <mpi.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

// #define CHECK_RESULT

#include "comm_exp.h"

#ifdef USE_ABT
#include "mv/helper_abt.h"
#elif defined(USE_PTH)
#include "mv/helper_pth.h"
#else
#include "mv/helper.h"
#endif

#include "mv/profiler.h"

#define CHECK_RESULT 0

#if 0
#undef total
#define total 20
#undef skip
#define skip 0
#endif

#define MIN_MSG_SIZE (131072*2)
#define MAX_MSG_SIZE (1 << 22)

int size = 0;

int main(int argc, char** args)
{
  MPIV_Init(&argc, &args);
  if (argc > 1) size = atoi(args[1]);
  MPIV_Start_worker(1, 0);
  MPIV_Finalize();
  return 0;
}

extern double mv_ptime;

void main_task(intptr_t arg)
{
  double times = 0;
  double ptime = 0;
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  void* r_buf = (void*) MPIV_Alloc((size_t)MAX_MSG_SIZE);
  void* s_buf = (void*) MPIV_Alloc((size_t)MAX_MSG_SIZE);

  for (int size = MIN_MSG_SIZE; size <= MAX_MSG_SIZE; size <<= 1) {
    int total = TOTAL;
    int skip = SKIP;

    if (size > LARGE) {
      total = TOTAL_LARGE;
      skip = SKIP_LARGE;
    }
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
      memset(r_buf, 'a', size);
      memset(s_buf, 'b', size);
      for (int t = 0; t < total + skip; t++) {
        if (t == skip) {
          times = MPI_Wtime();
        }
        MPIV_Send(s_buf, size, MPI_CHAR, 1, 1, MPI_COMM_WORLD);
        MPIV_Recv(r_buf, size, MPI_CHAR, 1, 2, MPI_COMM_WORLD,
                  MPI_STATUS_IGNORE);
        if (t == 0 || CHECK_RESULT) {
          for (int j = 0; j < size; j++) {
            assert(((char*)r_buf)[j] == 'b');
            assert(((char*)s_buf)[j] == 'b');
          }
        }
      }
      times = MPI_Wtime() - times;
      printf("[%d] %f\n", size, times * 1e6 / total / 2);//, (mv_ptime - ptime) * 1e6/ total / 2);
    } else {
      memset(s_buf, 'b', size);
      memset(r_buf, 'a', size);
      for (int t = 0; t < total + skip; t++) {
        MPIV_Recv(r_buf, size, MPI_CHAR, 0, 1, MPI_COMM_WORLD,
                  MPI_STATUS_IGNORE);
        MPIV_Send(s_buf, size, MPI_CHAR, 0, 2, MPI_COMM_WORLD);
      }
    }
    MPI_Barrier(MPI_COMM_WORLD);
  }

  MPIV_Free(r_buf);
  MPIV_Free(s_buf);
}
