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
  size_t heap_size = 128 * 1024 * 1024;
  lc_open(heap_size, &mv);

  if (argc > 1)
    WINDOWS = atoi(args[1]);

  int rank = lc_id(mv);
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
    void* rx_buffer;
    buffer = malloc(len);
    rx_buffer = malloc(len);
    memset(buffer, 'A', len);
    if (rank == 0) {
      double t1;
      for (int i = 0; i < skip + total; i++) {
        if (i == skip) t1 = wtime();
        for (int j = 0; j < WINDOWS; j++)  {
          while (!lc_send_tag(mv, buffer, len, 1, 0, &ctx))
            lc_progress(mv);
          while (!lc_test(&ctx))
            lc_progress(mv);
        }
        while (!lc_recv_tag(mv, rx_buffer, len, 1, 0, &ctx))
          lc_progress(mv);
        lc_recv_tag_post(mv, &ctx, 0);
        while (!lc_test(&ctx)) {
          lc_progress(mv);
        }
      }
      printf("%llu \t %.5f\n", len, (wtime() - t1)/total / (WINDOWS+1) * 1e6);
    } else {
      for (int i = 0; i < skip + total; i++) {
        //rx_buffer
        for (int j = 0; j < WINDOWS; j++) {
          while (!lc_recv_tag(mv, rx_buffer, len, 0, 0, &ctx))
            lc_progress(mv);
          lc_recv_tag_post(mv, &ctx, 0);
          while (!lc_test(&ctx)) {
            lc_progress(mv);
          }
        }
        // send
        while (!lc_send_tag(mv, buffer, len, 0, 0, &ctx))
          lc_progress(mv);
        while (!lc_test(&ctx))
          lc_progress(mv);
      }
    }
    free(rx_buffer);
    free(buffer);
  }
  lc_close(mv);
  return 0;
}
