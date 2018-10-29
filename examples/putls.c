#include "lc.h"
#include "comm_exp.h"

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** args) {
  lc_ep def, ep;
  lc_req req;
  lc_req* req_ptr;
  int rank;

  lc_init(1, &def);
  lc_opt opt = {.dev = 0, .desc = LC_EXPL_CQ};
  lc_ep_dup(&opt, def, &ep);

  lc_get_proc_num(&rank);

  uintptr_t addr, raddr;
  lc_ep_get_baseaddr(ep, MAX_MSG, &addr);

  lc_sendm(&addr,  sizeof(uintptr_t), 1-rank, 0, ep);
  lc_recvm(&raddr, sizeof(uintptr_t), 1-rank, 0, ep, &req);
  while (lc_cq_pop(ep, &req_ptr) != LC_OK)
    lc_progress(0);

  long* sbuf = (long*) addr;
  long* rbuf = (long*) (addr + MAX_MSG);
  memset(sbuf, 1, sizeof(char) * MAX_MSG);
  rbuf[0] = -1;
  lc_pm_barrier();

  double t1;
  lc_sync sync;

  for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
    for (int i = 0; i < TOTAL+SKIP; i++) {
      if (i == SKIP)
        t1 = wtime();
      if (rank == 0) {
        while (lc_cq_pop(ep, &req_ptr) != LC_OK)
          lc_progress(0);
        assert(req_ptr->meta == i);
        lc_cq_reqfree(ep, req_ptr);
        while (lc_putls(sbuf, size, 1-rank, raddr + MAX_MSG, i, ep, lc_signal, &sync) != LC_OK)
          lc_progress(0);
        while (!sync)
          lc_progress(0);
      } else {
        sync = 0;
        while (lc_putls(sbuf, size, 1-rank, raddr + MAX_MSG, i, ep, lc_signal, &sync) != LC_OK)
          lc_progress(0);
        while (!sync)
          lc_progress(0);
        while (lc_cq_pop(ep, &req_ptr) != LC_OK)
          lc_progress(0);
        assert(req_ptr->meta == i);
        lc_cq_reqfree(ep, req_ptr);
      }
    }

    if (rank == 0) {
      t1 = 1e6 * (wtime() - t1) / TOTAL / 2;
      printf("%10.d %10.3f\n", size, t1);
    }
  }

  lc_finalize();
}
