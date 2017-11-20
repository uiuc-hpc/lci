#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "lc.h"
#include "comm_exp.h"

lch* mv;
int WINDOWS = 1;

int main(int argc, char** args)
{
  lc_open(&mv, 1);

  if (argc > 1)
    WINDOWS = atoi(args[1]);

  int rank = lc_id(mv);
  int total, skip;
  void* buffer, *rx_buffer;
  posix_memalign(&buffer, 4096, 1<<22);
  posix_memalign(&rx_buffer, 4096, 1<<22);
  lc_req ctx;
  for (size_t len = 1; len <= (1 << 22); len <<= 1) {
    if (len > 8192) {
      skip = SKIP_LARGE;
      total = TOTAL_LARGE;
    } else {
      skip = SKIP;
      total = TOTAL;
    }
    memset(buffer, 'A', len);
    if (rank == 0) {
      double t1;
      for (int i = 0; i < skip + total; i++) {
        if (i == skip) t1 = wtime();
        for (int j = 0; j < WINDOWS; j++)  {
          lc_send_queue(mv, buffer, len, 1, i, 0, &ctx);
          lc_wait_poll(mv, &ctx);
        }
        int rank; lc_qtag tag; size_t size;
        while (!lc_recv_queue(mv, &size, &rank, &tag, 0, NULL, rx_buffer, &ctx)) {
          lc_progress(mv);
        }
        lc_wait_poll(mv, &ctx);
      }
      printf("%llu \t %.5f\n", len, (wtime() - t1)/total / (WINDOWS+1) * 1e6);
    } else {
      for (int i = 0; i < skip + total; i++) {
        int rank; lc_qtag tag; size_t size;
        //rx_buffer
        for (int j = 0; j < WINDOWS; j++) {
          while (!lc_recv_queue(mv, &size, &rank, &tag, 0, NULL, rx_buffer, &ctx)) {
            lc_progress(mv);
          }
          if (tag != i) {
              printf("%d (expect %d)\n", tag, i);
          }
          lc_wait_poll(mv, &ctx);
        }
        // send
        lc_send_queue(mv, buffer, len, 0, i, 0, &ctx);
        lc_wait_poll(mv, &ctx);
      }
    }
  }
  free(rx_buffer);
  free(buffer);
  lc_close(mv);
  return 0;
}
