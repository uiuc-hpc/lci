#include "lc.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "comm_exp.h"

int total = TOTAL;
int skip = SKIP;
void* buf;

static void* no_alloc(void* ctx, size_t size)
{
  return buf;
}

static void no_free(void* ctx, void* buf)
{
  return;
}


int main(int argc, char** args) {
  lc_ep ep[2];
  lc_rep rep[2];
  lc_dev dev[2];

  lc_init();

  int rank;
  lc_get_proc_num(&rank);

  lc_dev_open(&dev[0]);
  lc_dev_open(&dev[1]);

  lc_ep_open(dev[0], EP_TYPE_QUEUE, &ep[0]);
  lc_ep_open(dev[1], EP_TYPE_QUEUE, &ep[1]);
  lc_ep_set_alloc(ep[0], no_alloc, no_free, NULL);
  lc_ep_set_alloc(ep[1], no_alloc, no_free, NULL);

  lc_ep_query(dev[0], 1-rank, 0, &rep[0]);
  lc_ep_query(dev[1], 1-rank, 1, &rep[1]);
  LCI_barrier();

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
  double t1;
  size_t alignment = sysconf(_SC_PAGESIZE);
  posix_memalign(&buf, alignment, MAX_MSG);

  if (rank == 0) {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      wr.source_data.addr = buf;
      wr.source_data.size = size;
      wr.source = 0;
      
      if (size > LARGE) { total = TOTAL_LARGE; skip = SKIP_LARGE; }
      for (int i = 0; i < total + skip; i++) {
        if (i == skip) t1 = wtime();
        req.flag = 0;
        wr.meta.val = i;
        int id = i & 1;
        wr.target = rep[id];
        while (lc_submit(ep[id], &wr, &req) != LC_OK)
          { lc_progress_q(dev[id]); }
        while (req.flag == 0)
          { lc_progress_q(dev[id]); }

        while (lc_recv_qalloc(ep[id], &req) != LC_OK)
          { lc_progress_q(dev[id]); }
        assert(req.meta.val == i);
        lc_free(ep[id], req.buffer);
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

      if (size > LARGE) { total = TOTAL_LARGE; skip = SKIP_LARGE; }
      for (int i = 0; i < total + skip; i++) {
        int id = i & 1;
        wr.target = rep[id];
        while (lc_recv_qalloc(ep[id], &req) != LC_OK)
          { lc_progress_q(dev[id]); }
        assert(req.meta.val == i);
        lc_free(ep[id], req.buffer);

        req.flag = 0;
        wr.meta.val = i;
        while (lc_submit(ep[id], &wr, &req) != LC_OK)
          { lc_progress_q(dev[id]); }
        while (req.flag == 0)
          { lc_progress_q(dev[id]); }
      }
    }
  }
  lc_finalize(ep);
}
