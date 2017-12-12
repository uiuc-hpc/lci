#include "lc.h"
#include "comm_exp.h"

#include <assert.h>
#include <string.h>

#define MIN_MSG_SIZE 1
#define MAX_MSG_SIZE (1<<22)

lch* mv;

int main(int argc, char** args)
{
  lc_open(&mv, 0);
  char* buf = 0, *src = 0;
  posix_memalign((void*) &buf, 4096, MAX_MSG_SIZE);
  posix_memalign((void*) &src, 4096, MAX_MSG_SIZE);

  lc_addr rma, rma_remote;
  lc_rma_init(mv, buf, MAX_MSG_SIZE, &rma);
  int rank = lc_id(mv);
  lc_info info = {LC_SYNC_NULL, LC_SYNC_NULL, {{0}}};

  // Exchange the lc_addr.
  if (rank == 0) {
    lc_req s;
    lc_send_tag(mv, &rma, sizeof(lc_addr), 1, &info, &s);
    lc_wait_poll(mv, &s);

    lc_recv_tag(mv, &rma_remote, sizeof(lc_addr), 1, &info, &s);
    lc_wait_poll(mv, &s);
  } else {
    lc_req s;
    lc_recv_tag(mv, &rma_remote, sizeof(lc_addr), 0, &info, &s);
    lc_wait_poll(mv, &s);

    lc_send_tag(mv, &rma, sizeof(lc_addr), 0, &info, &s);
    lc_wait_poll(mv, &s);
  }

  printf("Done exchange address\n");
  memset(buf, 'B', 4);
  memset(buf + 4, 'A', MAX_MSG_SIZE);

  lc_req c1;
  info.offset = 4;

  for (int size = MIN_MSG_SIZE; size <= MAX_MSG_SIZE; size <<= 1) {
    double t1 = 0;
    for (int i = 0; i < TOTAL+SKIP; i++) {
      if (i == SKIP)
        t1 = wtime();

      LC_SAFE(lc_send_get(mv, src, size, &rma_remote, &info, &c1));
      lc_wait_poll(mv, &c1);

      if (i == 0) {
        for (int j = 0; j < size; j++)
          assert(src[j] == 'A');
      }
    }
    if (rank == 0) {
      t1 = wtime() - t1;
      printf("%d \t %.5f \n", size, t1 * 1e6 / TOTAL / 2);
    }
  }

  lc_rma_fini(mv, &rma);
  lc_close(mv);
}
