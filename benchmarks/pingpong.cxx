#include <stdio.h>
#include <thread>
#include <string.h>
#include <assert.h>
#include <atomic>
#include <sys/time.h>
#include <unistd.h>
#include <mpi.h>

// #define CHECK_RESULT

#include "mpiv.h"
#include "comm_queue.h"
#include "comm_exp.h"

#include "profiler.h"

#if 0
#undef total
#define total 20
#undef skip
#define skip 0
#endif

#define MIN_MSG_size 1
#define MAX_MSG_size 4*1024*1024

int main(int argc, char** args) {
  MPIV_Init(argc, args);
  MPIV_Init_worker(1);
  MPIV_Finalize();
  return 0;
}

void main_task(intptr_t arg) {
  double times = 0;
  int rank = MPIV.me;
  void* r_buf = (void*) mpiv_malloc((size_t) MAX_MSG_size + 64);
  void* s_buf = (void*) mpiv_malloc((size_t) MAX_MSG_size + 64);

  for (int size=MIN_MSG_size; size<=MAX_MSG_size; size<<=1) {
    int total = TOTAL;
    int skip = SKIP;

    if (size > LARGE) {
      total = TOTAL_LARGE;
      skip = SKIP_LARGE;
    }
    memset(s_buf, 'a', size);
    memset(r_buf, 'b', size);

    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
      for (int t = 0; t < total + skip; t++) {
        if (t == skip) {
          times = MPIV_Wtime();
        }
        MPIV_Send(s_buf, size, 1, 1);
        MPIV_Recv(r_buf, size, 1, 1);
      }
      times = MPIV_Wtime() - times;
      printf("[%d] %f\n", size, times * 1e6 / total / 2);
    } else {
      for (int t = 0; t < total + skip; t++) {
        MPIV_Recv(r_buf, size, 0, 1);
        MPIV_Send(s_buf, size, 0, 1);
      }
    }
  }

  mpiv_free(r_buf);
  mpiv_free(s_buf);
}
