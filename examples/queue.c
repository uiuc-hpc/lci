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
  lc_dev dev;
  lc_ep ep;
  lc_rep rep;

  lc_init();
  lc_dev_open(&dev);
  int rank;
  lc_get_proc_num(&rank);
  lc_ep_open(dev, EP_TYPE_QUEUE, &ep);
  lc_ep_set_alloc(ep, no_alloc, no_free, NULL);
  lc_ep_query(dev, 1-rank, 0, &rep);

  lc_req req;
  double t1;
  size_t alignment = sysconf(_SC_PAGESIZE);
  buf = malloc(MAX_MSG + alignment);
  buf = (void*)(((uintptr_t) buf + alignment - 1) / alignment * alignment);
  lc_meta meta = {99};

  if (rank == 0) {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      if (size > LARGE) { total = TOTAL_LARGE; skip = SKIP_LARGE; }
      for (int i = 0; i < total + skip; i++) {
        if (i == skip) t1 = wtime();

        meta.val = i;
        while (lc_send_qalloc(ep, rep, buf, size, meta, &req) != LC_OK)
          lc_progress_q(dev);
        while (req.flag == 0)
          lc_progress_q(dev);
        while (lc_recv_qalloc(ep, &req) != LC_OK)
          lc_progress_q(dev);

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
          lc_progress_q(dev);
        assert(req.meta.val == i);
        lc_free(ep, req.buffer);
        meta.val = i;
        while (lc_send_qalloc(ep, rep, buf, size, meta, &req) != LC_OK)
          lc_progress_q(dev);
        while (req.flag == 0)
          lc_progress_q(dev);
      }
    }
  }
  lc_finalize();
}
