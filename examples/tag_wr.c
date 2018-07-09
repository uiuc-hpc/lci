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
  lc_ep_open(hw, EP_TYPE_TAG, &ep);
  lc_ep_connect(hw, 1-lc_rank(), 0, &rep);

  lc_req req;
  struct lc_wr wr1, wr2;
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
     
      wr1.type = WR_PROD;
      wr1.source_data.addr = src_buf;
      wr1.source_data.size = size;
      wr1.target_data.type = DAT_EXPL;
      wr1.meta.val = 99;
      wr1.source = 0;
      wr1.target = rep;

      wr2.type = WR_CONS;
      wr2.target_data.addr = dst_buf;
      wr2.target_data.size = size;
      wr2.source_data.type = DAT_EXPL;
      wr2.meta.val = 99;
      wr2.source = 1;
      wr2.target = rep;

      if (size > LARGE) { total = TOTAL_LARGE; skip = SKIP_LARGE; }

      for (int i = 0; i < total + skip; i++) {
        if (i == skip) t1 = wtime();
        req.flag = 0;
        while (lc_submit(ep, &wr1, &req) != LC_OK)
          lc_progress_t(hw);
        while (req.flag == 0)
          lc_progress_t(hw);

        req.flag = 0;
        while (lc_submit(ep, &wr2, &req) != LC_OK)
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
      wr1.type = WR_PROD;
      wr1.source_data.addr = src_buf;
      wr1.source_data.size = size;
      wr1.target_data.type = DAT_EXPL;
      wr1.meta.val = 99;
      wr1.source = 1;
      wr1.target = rep;

      wr2.type = WR_CONS;
      wr2.target_data.addr = dst_buf;
      wr2.target_data.size = size;
      wr2.source_data.type = DAT_EXPL;
      wr2.meta.val = 99;
      wr2.source = 0;
      wr2.target = rep;
      if (size > LARGE) { total = TOTAL_LARGE; skip = SKIP_LARGE; }

      for (int i = 0; i < total + skip; i++) {
        req.flag = 0;
        while (lc_submit(ep, &wr2, &req) != LC_OK)
          lc_progress_t(hw);
        while (req.flag == 0)
          lc_progress_t(hw);

        req.flag = 0;
        while (lc_submit(ep, &wr1, &req) != LC_OK)
          lc_progress_t(hw);
        while (req.flag == 0)
          lc_progress_t(hw);

      }
    }
  }
  lc_finalize();
}
