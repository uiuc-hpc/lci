#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "mv.h"
#define MV_USE_SERVER_IBV
#include "src/include/mv_priv.h"

// #define USE_L1_MASK
#ifdef USE_ABT
#include "mv/helper_abt.h"
#elif defined(USE_PTH)
#include "mv/helper_pth.h"
#else
#include "mv/helper.h"
#endif

#include "comm_exp.h"

mvh* mv;

int main(int argc, char** args)
{
  size_t heap_size = 1024 * 1024 * 1024;
  mv_open(&argc, &args, heap_size, &mv);
  set_me_to_last();

  int rank, len;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  len = atoi(args[1]);

  mv_ctx ctx;
  if (rank == 0) {
    void* buffer = mv_alloc(mv, len);
    memset(buffer, 'A', len);
    double t1; 
    for (int i = 0; i < SKIP_LARGE + TOTAL_LARGE; i++) {
      if (i == SKIP_LARGE) t1 = MPI_Wtime();
      mv_send_rdz_enqueue_init(mv, buffer, len, 1, 0, &ctx);
      while (!mv_test(&ctx))
        mv_progress(mv);
    }
    printf("Latency: %.5f\n", (MPI_Wtime() - t1)/TOTAL_LARGE* 1e6);
  } else {
    for (int i = 0; i < SKIP_LARGE + TOTAL_LARGE; i++) {
      while (!mv_recv_dequeue(mv, &ctx)) {
        mv_progress(mv);
      }
      if (i == 0)
        for (int j = 0; j < len; j++)
          assert(((char*)ctx.buffer)[j] == 'A');
      mv_free(mv, ctx.buffer);
      mv_packet_done(mv, (mv_packet*) ctx.control);
    }
  }
  
  mv_close(mv);
  return 0;
}

void main_task(intptr_t arg) { }

