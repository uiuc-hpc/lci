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
  lc_open(&mv);

  int rank = lc_id(mv);
  int total, skip;
  void* buffer, *rx_buffer;
  posix_memalign(&buffer, 4096, 1<<22);
  posix_memalign(&rx_buffer, 4096, 1<<22);
  size_t len = atoi(args[1]);
  for (size_t WINDOWS = 1; WINDOWS <= 1024; WINDOWS <<= 1) {
    lc_req ctx[WINDOWS];
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
          lc_send_queue(mv, buffer, len, 1, 0, &ctx[j]);
        }
        for (int j = 0; j < WINDOWS; j++)  {
          lc_wait_poll(mv, &ctx[j]);
        }
        int rank, tag, size;
        while (!lc_recv_queue_probe(mv, &size, &rank, &tag, &ctx)) {
          lc_progress(mv);
        }
        lc_recv_queue(mv, rx_buffer, &ctx);
        lc_wait_poll(mv, &ctx);
      }
      t1 = wtime() - t1;
      printf("%d \t %.5f %.5f \n", WINDOWS, total * (WINDOWS+1) / t1, total * (WINDOWS+1) * len / 1e6 / t1);
    } else {
      for (int i = 0; i < skip + total; i++) {
        int rank, tag, size;
        //rx_buffer
        for (int j = 0; j < WINDOWS; j++) {
          while (!lc_recv_queue_probe(mv, &size, &rank, &tag, &ctx[j])) {
            lc_progress(mv);
          }
          lc_recv_queue(mv, rx_buffer, &ctx[j]);
        }
        for (int j = 0; j < WINDOWS; j++) {
          lc_wait_poll(mv, &ctx[j]);
        }

        // send
        lc_send_queue(mv, buffer, len, 0, i, &ctx);
        lc_wait_poll(mv, &ctx);
      }
    }
  }
  free(rx_buffer);
  free(buffer);
  lc_close(mv);
  return 0;
}
