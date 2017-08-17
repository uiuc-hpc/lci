#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

// #define CHECK_RESULT

#include "mpiv.h"
#include "comm_exp.h"

#if 0
#ifdef USE_ABT
#include "ult/helper_abt.h"
#elif defined(USE_PTH)
#include "ult/helper_pth.h"
#else
#include "ult/helper.h"
#endif
#endif

#include "lc/profiler.h"

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
  MPI_Init(&argc, &args);
  if (argc > 1) size = atoi(args[1]);
  MPI_Start_worker(1);
  set_me_to(14);
  double times = 0;
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  void* r_buf = memalign(4096, (size_t)MAX_MSG_SIZE);
  void* s_buf = memalign(4096, (size_t)MAX_MSG_SIZE);

  for (int size = MIN_MSG_SIZE; size <= MAX_MSG_SIZE; size <<= 1) {
    int total = TOTAL;
    int skip = SKIP;

    if (size > LARGE) {
      total = TOTAL_LARGE;
      skip = SKIP_LARGE;
    }
    // MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
      memset(r_buf, 'a', size);
      memset(s_buf, 'b', size);
      for (int t = 0; t < total + skip; t++) {
        if (t == skip) {
          times = wtime();
        }
        MPI_Send(s_buf, size, MPI_CHAR, 1, 1, MPI_COMM_WORLD);
        MPI_Recv(r_buf, size, MPI_CHAR, 1, 2, MPI_COMM_WORLD,
                  MPI_STATUS_IGNORE);
        if (t == 0 || CHECK_RESULT) {
          for (int j = 0; j < size; j++) {
            assert(((char*)r_buf)[j] == 'b');
            assert(((char*)s_buf)[j] == 'b');
          }
        }
      }
      times = wtime() - times;
      printf("[%d] %f\n", size, times * 1e6 / total / 2);//, (mv_ptime - ptime) * 1e6/ total / 2);
    } else {
      memset(s_buf, 'b', size);
      memset(r_buf, 'a', size);
      for (int t = 0; t < total + skip; t++) {
        MPI_Recv(r_buf, size, MPI_CHAR, 0, 1, MPI_COMM_WORLD,
                  MPI_STATUS_IGNORE);
        MPI_Send(s_buf, size, MPI_CHAR, 0, 2, MPI_COMM_WORLD);
      }
      // printf("[%d] %f\n", size, rx_time * 1e6 / total / 2);//, (mv_ptime - ptime) * 1e6/ total / 2);
    }
    // MPI_Barrier(MPI_COMM_WORLD);
  }

  free(r_buf);
  free(s_buf);

  MPI_Stop_worker();
  MPI_Finalize();
}
