#include "lc.h"
#include "../comm_exp.h"

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#undef MAX_MSG
#define MAX_MSG 128

int main(int argc, char** args)
{
  lc_ep ep;
  lc_req req;
  int rank;

  lc_init(1, &ep);
  lc_get_proc_num(&rank);

  uintptr_t addr, raddr;
  lc_ep_get_baseaddr(ep, MAX_MSG, &addr);

  lc_sendm(&addr, sizeof(uintptr_t), 1 - rank, 0, ep);
  lc_recvm(&raddr, sizeof(uintptr_t), 1 - rank, 0, ep, &req);
  while (!req.sync) {
    lc_progress(0);
  }

  long* sbuf = (long*)addr;
  long* rbuf = (long*)(addr + MAX_MSG);
  memset(sbuf, 1, sizeof(char) * MAX_MSG);
  rbuf[0] = -1;
  LCI_barrier();

  double t1;

  for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
    for (int i = 0; i < TOTAL + SKIP; i++) {
      if (i == SKIP) t1 = wtime();
      if (rank == 0) {
        while (rbuf[0] == -1) lc_progress(0);
        rbuf[0] = -1;
        lc_puts(sbuf, size, 1 - rank, raddr + MAX_MSG, ep);
      } else {
        lc_puts(sbuf, size, 1 - rank, raddr + MAX_MSG, ep);
        while (rbuf[0] == -1) lc_progress(0);
        rbuf[0] = -1;
      }
    }

    if (rank == 0) {
      t1 = 1e6 * (wtime() - t1) / TOTAL / 2;
      printf("%10.d %10.3f\n", size, t1);
    }
  }

  lc_finalize();
}
