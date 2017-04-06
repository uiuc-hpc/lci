#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "lc.h"

// #define USE_L1_MASK
#ifdef USE_ABT
#include "mv/helper_abt.h"
#elif defined(USE_PTH)
#include "mv/helper_pth.h"
#else
#include "mv/helper.h"
#endif

#include "comm_exp.h"

lch* mv;
int WINDOWS = 1;

int main(int argc, char** args)
{
  size_t heap_size = 128 * 1024 * 1024;
  lc_open(heap_size, &mv);

  if (argc > 1)
    WINDOWS = atoi(args[1]);

  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  int total, skip;

  lc_ctx ctx;
  for (size_t len = 1; len <= (1 << 22); len <<= 1) {
    if (len > 8192) {
      skip = SKIP_LARGE;
      total = TOTAL_LARGE;
    } else {
      skip = SKIP;
      total = TOTAL;
    }
    void* buffer;
    void* recv;
    buffer = malloc(len);
    recv = malloc(len);
    memset(buffer, 'A', len);
    if (rank == 0) {
      double t1;
      for (int i = 0; i < skip + total; i++) {
        if (i == skip) t1 = MPI_Wtime();
        for (int j = 0; j < WINDOWS; j++)  {
          while (!lc_send_queue(mv, buffer, len, 1, 0, &ctx))
            lc_progress(mv);
          while (!lc_test(&ctx))
            lc_progress(mv);
        }

        int rank, tag, size;
        while (!lc_recv_queue(mv, &size, &rank, &tag, &ctx)) {
          lc_progress(mv);
        }
        lc_recv_queue_post(mv, recv, &ctx);
        while (!lc_test(&ctx)) {
          lc_progress(mv);
        }
        if (i == -1) {
          assert(rank == 1);
          assert(tag == i);
          assert(size == len);
          for (int j = 0; j < size; j++) {
            assert(((char*)recv)[j] == 'A');
          }
        }
      }
      printf("%d \t %.5f\n", len, (MPI_Wtime() - t1)/total / (WINDOWS+1) * 1e6);
    } else {
      for (int i = 0; i < skip + total; i++) {
        int rank, tag, size;
        //recv
        for (int j = 0; j < WINDOWS; j++) {
          while (!lc_recv_queue(mv, &size, &rank, &tag, &ctx)) {
            lc_progress(mv);
          }
          lc_recv_queue_post(mv, recv, &ctx);
          while (!lc_test(&ctx)) {
            lc_progress(mv);
          }
        }
        // send
        while (!lc_send_queue(mv, buffer, len, 0, i, &ctx))
          lc_progress(mv);
        // lc_send_enqueue_post(mv, &ctx, 0);
        while (!lc_test(&ctx))
          lc_progress(mv);
      }
    }
    MPI_Barrier(MPI_COMM_WORLD);
    free(recv);
    free(buffer);
  }
  lc_close(mv);
  return 0;
}

void main_task(intptr_t arg) { }

