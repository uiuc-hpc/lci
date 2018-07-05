#include "lc.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "comm_exp.h"

int total = TOTAL;
int skip = SKIP;

int main(int argc, char** args) {
  lc_init(1);
  lc_ep ep[2];
  lc_rep rep[2];
  lc_ep_open(0, EP_TYPE_QUEUE, &ep[0]);
  lc_ep_open(0, EP_TYPE_QUEUE, &ep[1]);

  lc_ep_connect(0, 1-lc_rank(), 0, &rep[0]);
  lc_ep_connect(0, 1-lc_rank(), 1, &rep[1]);

  PMI_Barrier();

  lc_req req;
  struct lc_wr wr = {
    .type = WR_PROD,
    .source_data = {
      .type = DAT_EXPL,
      .addr = 0,
      .size = 0
    },
    .target_data = {
      .type = DAT_ALLOC,
      .alloc_id = 0,
      .alloc_ctx = 0
    },
    .source = 0,
    .target = 0,
    .meta = {99},
  };
  struct lc_sig sig = {SIG_CQ};
  double t1;
  size_t alignment = sysconf(_SC_PAGESIZE);
  void* buf = malloc(MAX_MSG + alignment);
  buf = (void*)(((uintptr_t) buf + alignment - 1) / alignment * alignment);

  if (lc_rank() == 0) {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      wr.source_data.addr = buf;
      wr.source_data.size = size;
      wr.source = 0;
      wr.target = rep[1];
      
      if (size > LARGE) { total = TOTAL_LARGE; skip = SKIP_LARGE; }
      for (int i = 0; i < total + skip; i++) {
        if (i == skip) t1 = wtime();
        req.flag = 0;
        wr.meta.val = i;
        while (lc_submit(ep[0], &wr, &req) != LC_OK)
          lc_progress_q();
        while (req.flag == 0)
          lc_progress_q();
        while (lc_deq_alloc(ep[1], &req) != LC_OK)
          lc_progress_q();
        assert(req.meta.val == i);
        lc_free(ep[1], req.buffer);
      }

      t1 = 1e6 * (wtime() - t1) / total / 2;
      printf("%10.d %10.3f\n", size, t1);
    }
  } else {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      memset(buf, 'a', size);
      wr.source_data.addr = buf;
      wr.source_data.size = size;
      wr.source = 1;
      wr.target = rep[1];

      if (size > LARGE) { total = TOTAL_LARGE; skip = SKIP_LARGE; }
      for (int i = 0; i < total + skip; i++) {
        // req.flag = 0;
        while (lc_deq_alloc(ep[1], &req) != LC_OK)
          lc_progress_q();
        assert(req.meta.val == i);
        lc_free(ep[1], req.buffer);
        req.flag = 0;
        wr.meta.val = i;
        while (lc_submit(ep[0], &wr, &req) != LC_OK)
          lc_progress_q();
        while (req.flag == 0)
          lc_progress_q();
      }
    }
  }
  lc_finalize(ep);
}
