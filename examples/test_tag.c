#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "lc.h"
#include "comm_exp.h"

int main(int argc, char** args)
{
  lch *mv;
  lc_open(&mv);

  int rank = lc_id(mv);
  void* buffer = malloc(4096);
  void* rx_buffer = malloc(4096);
  lc_req ctx;
  int len = 4096;

  if (rank == 0) {
    LC_SAFE(lc_send_tag(mv, buffer, len-1, 1, 99, &ctx));
    lc_wait_poll(mv, &ctx);
  } else {
    lc_recv_tag(mv, rx_buffer, len, 0, 99, &ctx);
    lc_wait_poll(mv, &ctx);
    assert(ctx.size == len - 1);
  }

  free(rx_buffer);
  free(buffer);
  lc_close(mv);
  return 0;
}
