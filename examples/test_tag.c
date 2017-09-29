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

  if (argc > 1)
    WINDOWS = atoi(args[1]);

  int rank = lc_id(mv);
  int total, skip;
  void* buffer = lc_memalign(4096, 1 << 22);
  void* rx_buffer = lc_memalign(4096, 1 << 22);
  assert(buffer);
  assert(rx_buffer);
  lc_req ctx;
  int len = 1 << 22;

  if (rank == 0) {
    lc_send_tag(mv, buffer, len - 1, 1, 99, &ctx);
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
