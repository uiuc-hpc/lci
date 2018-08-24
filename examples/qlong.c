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
  lc_ep ep;
  lc_init(0, EP_AR_ALLOC, EP_CE_CQ, &ep);
  lc_ep_set_alloc(ep, no_alloc, no_free, NULL);

  int rank = 0;
  lc_get_proc_num(&rank);
  lc_meta meta = {99};

  lc_req req;
  double t1;
  size_t alignment = sysconf(_SC_PAGESIZE);
  buf = malloc(MAX_MSG);
  buf = (void*)(((uintptr_t) buf + alignment - 1) / alignment * alignment);
  void* sbuf = malloc(MAX_MSG);

  if (rank == 0) {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      memset(sbuf, 'a', size);

      if (size > LARGE) { total = TOTAL_LARGE; skip = SKIP_LARGE; }

      for (int i = 0; i < total + skip; i++) {
        if (i == skip) t1 = wtime();
        meta.val = i;
        req.flag = 0;
        while (lc_putld(ep, 1-rank, sbuf, size, meta, &req) != LC_OK)
          lc_progress_q(0);
        while (req.flag == 0)
          lc_progress_q(0);
        req.flag = 0;
        while (lc_cq_popval(ep, &req) != LC_OK)
          lc_progress_q(0);
        assert(req.meta.val == i);
        lc_free(ep, req.buffer);
      }

      t1 = 1e6 * (wtime() - t1) / total / 2;
      printf("%10.d %10.3f\n", size, t1);
    }
  } else {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      memset(sbuf, 'a', size);
      if (size > LARGE) { total = TOTAL_LARGE; skip = SKIP_LARGE; }

      for (int i = 0; i < total + skip; i++) {
        req.flag = 0;
        while (lc_cq_popval(ep, &req) != LC_OK)
          lc_progress_q(0);
        assert(req.meta.val == i);
        lc_free(ep, req.buffer);

        meta.val = i;
        req.flag = 0;
        while (lc_putld(ep, 1-rank, sbuf, size, meta, &req) != LC_OK)
          lc_progress_q(0);
        while (req.flag == 0)
          lc_progress_q(0);
      }
    }
  }
  lc_finalize();
}
