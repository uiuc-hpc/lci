#include <assert.h>
#include <mpi.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

// #define CHECK_RESULT

#include "comm_exp.h"
#include "helper.h"
#include "mv.h"

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
#define WIN 64

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
  void* r_buf = (void*)MPIV_Alloc((size_t)MAX_MSG_SIZE);
  void* s_buf = (void*)MPIV_Alloc((size_t)MAX_MSG_SIZE);

  for (int size = MIN_MSG_SIZE; size <= MAX_MSG_SIZE; size <<= 1) {
    int total = TOTAL;
    int skip = SKIP;

    if (size > LARGE) {
      total = TOTAL_LARGE;
      skip = SKIP_LARGE;
    }
    MPI_Barrier(MPI_COMM_WORLD);
    MPIV_Request r[64];
    if (rank == 0) {
      memset(r_buf, 'a', size);
      memset(s_buf, 'b', size);
      for (int t = 0; t < total + skip; t++) {
        if (t == skip) {
          times = MPI_Wtime();
        }
        for (int k = 0; k < WIN; k++) {
          MPIV_Isend(s_buf, size, MPI_CHAR, 1, k, MPI_COMM_WORLD, &r[k]);
          // MPIV_Send(s_buf, size, MPI_CHAR, 1, k, MPI_COMM_WORLD);
        }
        MPIV_Waitall(WIN, r, MPI_STATUSES_IGNORE);
        MPIV_Recv(r_buf, 4, MPI_CHAR, 1, WIN + 1, MPI_COMM_WORLD,
                  MPI_STATUS_IGNORE);
      }
      times = MPI_Wtime() - times;
      printf("[%d] %f\n", size, (1e-6 * size * total * WIN) / times);
    } else {
      memset(s_buf, 'b', size);
      memset(r_buf, 'a', size);
      for (int t = 0; t < total + skip; t++) {
        for (int k = 0; k < WIN; k++) {
          MPIV_Irecv(r_buf, size, MPI_CHAR, 0, k, MPI_COMM_WORLD, &r[k]);
          // MPIV_Recv(r_buf, size, MPI_CHAR, 0, k, MPI_COMM_WORLD, MPI_STATUS_IGNORE);//, &r[k]);
        }
        MPIV_Waitall(WIN, r, MPI_STATUSES_IGNORE);
        MPIV_Send(s_buf, 4, MPI_CHAR, 0, WIN + 1, MPI_COMM_WORLD);
        if (t == 0 || CHECK_RESULT) {
          for (int j = 0; j < size; j++) {
            assert(((char*)r_buf)[j] == 'b');
          }
        }
      }
    }
    MPI_Barrier(MPI_COMM_WORLD);
  }

  MPIV_Free(r_buf);
  MPIV_Free(s_buf);
}
