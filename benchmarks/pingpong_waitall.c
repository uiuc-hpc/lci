#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "mpiv.h"
#include "comm_exp.h"
#include "lc.h"

#ifdef USE_ABT
#include "ult/helper_abt.h"
#elif defined(USE_PTH)
#include "ult/helper_pth.h"
#else
#include "ult/helper.h"
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
int WIN = 64;

int main(int argc, char** args)
{
  MPI_Init(&argc, &args);
  MPI_Start_worker(1);
  set_me_to(14);

  double times = 0;
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  void* r_buf = (void*)malloc((size_t)MAX_MSG_SIZE);
  void* s_buf = (void*)malloc((size_t)MAX_MSG_SIZE);
  // for (WIN = 128; WIN <= 128; WIN *= 2)
  for (int size = MIN_MSG_SIZE; size <= MAX_MSG_SIZE; size <<= 1) {
    int total = TOTAL;
    int skip = SKIP;

    if (size > LARGE) {
      total = TOTAL_LARGE;
      skip = SKIP_LARGE;
    }
    // MPI_Barrier(MPI_COMM_WORLD);
    MPI_Request r[WIN];
    if (rank == 0) {
      memset(r_buf, 'a', size);
      memset(s_buf, 'b', size);
      for (int t = 0; t < total + skip; t++) {
        if (t == skip) {
          times = wtime();
        }
        for (int k = 0; k < WIN; k++) {
          MPI_Isend(s_buf, size, MPI_CHAR, 1, k, MPI_COMM_WORLD, &r[k]);
          // MPI_Send(s_buf, size, MPI_CHAR, 1, k, MPI_COMM_WORLD);
        }
        MPI_Waitall(WIN, r, 0);
        MPI_Recv(r_buf, 4, MPI_CHAR, 1, WIN + 1, MPI_COMM_WORLD,
                 0);
      }
      times = wtime() - times;
      printf("%d %f\n", size, (total * WIN) / times);
      // printf("%d %f\n", size, size / 1e6 * total * WIN / times);
    } else {
      memset(s_buf, 'b', size);
      memset(r_buf, 'a', size);
      for (int t = 0; t < total + skip; t++) {
        for (int k = 0; k < WIN; k++) {
          MPI_Irecv(r_buf, size, MPI_CHAR, 0, k, MPI_COMM_WORLD, &r[k]);
          // MPI_Recv(r_buf, size, MPI_CHAR, 0, k, MPI_COMM_WORLD, MPI_STATUS_IGNORE);//, &r[k]);
        }
        MPI_Waitall(WIN, r, 0);
        MPI_Send(s_buf, 4, MPI_CHAR, 0, WIN + 1, MPI_COMM_WORLD);
        if (t == 0 || CHECK_RESULT) {
          for (int j = 0; j < size; j++) {
            // assert(((char*)r_buf)[j] == 'b');
          }
        }
      }
    }
    // MPI_Barrier(MPI_COMM_WORLD);
  }

  free(r_buf);
  free(s_buf);
  MPI_Stop_worker();
  MPI_Finalize();
}
