#include "lc.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "comm_exp.h"

int total = TOTAL;
int skip = SKIP;

int main(int argc, char** args) {
  lc_ep ep;
  lc_rep rep;
  lc_hw hw;

  lc_init();

  lc_hw_open(&hw);
  lc_ep_open(hw, EP_TYPE_QUEUE, &ep);
  lc_ep_connect(hw, 1-lc_rank(), 0, &rep);

  double t1;
  size_t alignment = sysconf(_SC_PAGESIZE);
  void* buf = malloc(MAX_MSG + alignment);
  buf = (void*)(((uintptr_t) buf + alignment - 1) / alignment * alignment);
  lc_req req;
  lc_meta meta = {99};

  if (lc_rank(ep) == 0) {
    for (int size = MIN_MSG; size <= 4096; size <<= 1) {
      
      if (size > LARGE) { total = TOTAL_LARGE; skip = SKIP_LARGE; }
      for (int i = 0; i < total + skip; i++) {
        if (i == skip) t1 = wtime();
        req.flag = 0;
        meta.val = i;
        while (lc_send_qshort(ep, rep, buf, size, meta, &req) != LC_OK)
          lc_progress_sq(hw);
        while (req.flag == 0)
          lc_progress_sq(hw);
        lc_req* req_ptr;
        while (lc_recv_qshort(ep, &req_ptr) != LC_OK)
          lc_progress_sq(hw);
        lc_req_free(ep, req_ptr);
      }

      t1 = 1e6 * (wtime() - t1) / total / 2;
      printf("%10.d %10.3f\n", size, t1);
    }
  } else {
    for (int size = MIN_MSG; size <= 4096; size <<= 1) {
      memset(buf, 'a', size);
      if (size > LARGE) { total = TOTAL_LARGE; skip = SKIP_LARGE; }
      for (int i = 0; i < total + skip; i++) {
        // req.flag = 0;
        lc_req* req_ptr;
        while (lc_recv_qshort(ep, &req_ptr) != LC_OK)
          lc_progress_sq(hw);
        lc_req_free(ep, req_ptr);
        req.flag = 0;
        meta.val = i;
        while (lc_send_qshort(ep, rep, buf, size, meta, &req) != LC_OK)
          lc_progress_sq(hw);
        while (req.flag == 0)
          lc_progress_sq(hw);
      }
    }
  }
  lc_finalize();
}
