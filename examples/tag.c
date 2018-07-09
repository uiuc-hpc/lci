#include "lc.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "comm_exp.h"

int total = TOTAL;
int skip = SKIP;

int main(int argc, char** args) {
  lc_init();

  lc_ep ep;
  lc_rep rep;
  lc_hw hw;

  lc_hw_open(&hw);
  lc_ep_open(hw, EP_TYPE_TAG, &ep);
  lc_ep_connect(hw, 1-lc_rank(), 0, &rep);
  lc_meta tag = {99};

  lc_req req;
  double t1;
  size_t alignment = sysconf(_SC_PAGESIZE);
  void* src_buf = malloc(MAX_MSG + alignment);
  void* dst_buf = malloc(MAX_MSG + alignment);
  src_buf = (void*)(((uintptr_t) src_buf + alignment - 1) / alignment * alignment);
  dst_buf = (void*)(((uintptr_t) dst_buf + alignment - 1) / alignment * alignment);

  if (lc_rank(ep) == 0) {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      memset(src_buf, 'a', size);
      memset(dst_buf, 'b', size);

      if (size > LARGE) { total = TOTAL_LARGE; skip = SKIP_LARGE; }

      for (int i = 0; i < total + skip; i++) {
        if (i == skip) t1 = wtime();
        req.flag = 0;
        while (lc_send_tag(ep, rep, src_buf, size, tag, &req) != LC_OK)
          lc_progress_t(hw);
        while (req.flag == 0)
          lc_progress_t(hw);

        req.flag = 0;
        while (lc_recv_tag(ep, rep, dst_buf, size, tag, &req) != LC_OK)
          lc_progress_t(hw);
        while (req.flag == 0)
          lc_progress_t(hw);
      }

      t1 = 1e6 * (wtime() - t1) / total / 2;
      printf("%10.d %10.3f\n", size, t1);
    }
  } else {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      memset(src_buf, 'a', size);
      memset(dst_buf, 'b', size);
      if (size > LARGE) { total = TOTAL_LARGE; skip = SKIP_LARGE; }

      for (int i = 0; i < total + skip; i++) {
        req.flag = 0;
        while (lc_recv_tag(ep, rep, dst_buf, size, tag, &req) != LC_OK)
          lc_progress_t(hw);
        while (req.flag == 0)
          lc_progress_t(hw);

        req.flag = 0;
        while (lc_send_tag(ep, rep, src_buf, size, tag, &req) != LC_OK)
          lc_progress_t(hw);
        while (req.flag == 0)
          lc_progress_t(hw);

      }
    }
  }
  lc_finalize();
}
