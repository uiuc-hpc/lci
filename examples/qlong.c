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
  lc_ep ep, ep_q;
  lc_init(1, &ep);
  lc_opt opt = {.dev = 0, .desc = LC_ALLOC_CQ, .alloc = no_alloc, .free = no_free};
  lc_ep_dup(&opt, ep, &ep_q);

  int rank = 0;
  lc_get_proc_num(&rank);
  lc_meta meta = {99};

  lc_req req;
  lc_sync sync;
  double t1;
  size_t alignment = sysconf(_SC_PAGESIZE);
  void* sbuf;
  posix_memalign(&buf, alignment, MAX_MSG);
  posix_memalign(&sbuf, alignment, MAX_MSG);

  if (rank == 0) {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      memset(sbuf, 'a', size);

      if (size > LARGE) { total = TOTAL_LARGE; skip = SKIP_LARGE; }

      for (int i = 0; i < total + skip; i++) {
        if (i == skip) t1 = wtime();
        meta = i;
        sync = 0;
        while (lc_sendl(sbuf, size, 1-rank, meta, ep_q, lc_signal, &sync) != LC_OK)
          lc_progress_q(0);
        while (sync == 0)
          lc_progress_q(0);

        lc_req* req_ptr;
        while (lc_cq_pop(ep_q, &req_ptr) != LC_OK)
          lc_progress_q(0);
        assert(req_ptr->meta == i);
        lc_free(ep_q, req_ptr->buffer);
        lc_cq_reqfree(ep_q, req_ptr);
      }

      t1 = 1e6 * (wtime() - t1) / total / 2;
      printf("%10.d %10.3f\n", size, t1);
    }
  } else {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      memset(sbuf, 'a', size);
      if (size > LARGE) { total = TOTAL_LARGE; skip = SKIP_LARGE; }

      for (int i = 0; i < total + skip; i++) {
        lc_req* req_ptr;
        while (lc_cq_pop(ep_q, &req_ptr) != LC_OK)
          lc_progress_q(0);
        assert(req_ptr->meta == i);
        lc_free(ep_q, req_ptr->buffer);
        lc_cq_reqfree(ep_q, req_ptr);

        meta = i;
        sync = 0;
        while (lc_sendl(sbuf, size, 1-rank, meta, ep_q, lc_signal, &sync) != LC_OK)
          lc_progress_q(0);
        while (sync == 0)
          lc_progress_q(0);
      }
    }
  }
  lc_finalize();
}
