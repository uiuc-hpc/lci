#include "lc.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "comm_exp.h"

int total = TOTAL;
int skip = SKIP;

int main(int argc, char** args) {
  lc_hw hw;
  lc_ep ep;
  lc_rep rep;

  lc_init();
  lc_hw_open(&hw);
  lc_ep_open(hw, EP_TYPE_QUEUE, &ep);
  lc_ep_connect(ep, 1-lc_rank(), 0, &rep);

  lc_req req;
  struct lc_sig sig = {SIG_CQ};
  double t1;
  size_t alignment = sysconf(_SC_PAGESIZE);
  void* buf = malloc(MAX_MSG + alignment);
  buf = (void*)(((uintptr_t) buf + alignment - 1) / alignment * alignment);
  lc_meta meta = {99};

  if (lc_rank() == 0) {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      if (size > LARGE) { total = TOTAL_LARGE; skip = SKIP_LARGE; }
      for (int i = 0; i < total + skip; i++) {
        if (i == skip) t1 = wtime();

        meta.val = i;
        while (lc_send_qalloc(ep, rep, buf, size, meta, &req) != LC_OK)
          lc_progress_q(hw);
        while (req.flag == 0)
          lc_progress_q(hw);
        while (lc_recv_qalloc(ep, &req) != LC_OK)
          lc_progress_q(hw);

        assert(req.meta.val == i);
        lc_free(ep, req.buffer);
      }

      t1 = 1e6 * (wtime() - t1) / total / 2;
      printf("%10.d %10.3f\n", size, t1);
    }
  } else {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      memset(buf, 'a', size);
      if (size > LARGE) { total = TOTAL_LARGE; skip = SKIP_LARGE; }
      for (int i = 0; i < total + skip; i++) {
        while (lc_recv_qalloc(ep, &req) != LC_OK)
          lc_progress_q(hw);
        assert(req.meta.val == i);
        lc_free(ep, req.buffer);
        meta.val = i;
        while (lc_send_qalloc(ep, rep, buf, size, meta, &req) != LC_OK)
          lc_progress_q(hw);
        while (req.flag == 0)
          lc_progress_q(hw);
      }
    }
  }
  lc_finalize();
}
