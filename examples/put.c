#include "lc.h"
#include "comm_exp.h"

#include <assert.h>

#define MIN_MSG_SIZE 1
#define MAX_MSG_SIZE (1<<22)

lch* mv;

int main(int argc, char** args)
{
  lc_open(&mv);
  char* buf = (char*) memalign(4096, MAX_MSG_SIZE);
  lc_addr rma;
  lc_rma_init(mv, buf, MAX_MSG_SIZE, &rma);
  int rank = lc_id(mv);

  void* buf2;
  if (rank == 0) {
    lc_req s;
    lc_send_queue(mv, &rma, sizeof(lc_addr), 1, 0, &s);
    lc_wait_poll(mv, &s);

    int size, rank, tag;
    lc_req r;
    while (!lc_recv_queue_probe(mv, &size, &rank, &tag, &r))
      lc_progress(mv);
    buf2 = malloc(size);
    lc_recv_queue(mv, buf2, &r);
  } else {
    int size, rank, tag;
    lc_req r;
    while (!lc_recv_queue_probe(mv, &size, &rank, &tag, &r))
      lc_progress(mv);
    buf2 = malloc(size);
    lc_recv_queue(mv, buf2, &r);

    lc_req s;
    lc_send_queue(mv, &rma, sizeof(lc_addr), 0, 0, &s);
    lc_wait_poll(mv, &s);
  }

  void* src = memalign(4096, MAX_MSG_SIZE);
  memset(src, 'A', MAX_MSG_SIZE);

  lc_addr* remote_rma = (lc_addr*) buf2;
  lc_req c1, c2;
  lc_recv_put(mv, &rma, &c2);

  for (int size = MIN_MSG_SIZE; size <= MAX_MSG_SIZE; size <<= 1) {
    double t1 = 0;
    for (int i = 0; i < TOTAL+SKIP; i++) {
      if (rank == 0) {
        if (i == SKIP)
          t1 = wtime();

        lc_recv_put(mv, &rma, &c1);
        lc_send_put(mv, src, size, 1, remote_rma, &c2);

        lc_wait_poll(mv, &c2);
        lc_wait_poll(mv, &c1);

        if (i == 0)
          for (int j = 0; j < size; j++)
            assert(buf[j] == 'A');
      } else {
        lc_wait_poll(mv, &c2);
        if (i == 0)
          for (int j = 0; j < size; j++)
            assert(buf[j] == 'A');

        lc_recv_put(mv, &rma, &c2);
        lc_send_put(mv, src, size, 0, remote_rma, &c1);
        lc_wait_poll(mv, &c1);
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
