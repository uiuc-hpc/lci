#include "lc.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "comm_exp.h"

#undef MAX_MSG
#define MAX_MSG (8 * 1024)

int total = TOTAL;
int skip = SKIP;
void* buf;

static void* alloc(size_t size, void* ctx) { return buf; }

int main(int argc, char** args)
{
  lc_ep ep, ep_q;
  lc_init(1, &ep);
  lc_opt opt = {.dev = 0, .desc = LC_DYN_CQ, .alloc = alloc};
  lc_ep_dup(&opt, ep, &ep_q);

  int rank = 0;
  lc_get_proc_num(&rank);
  int meta = {99};

  lc_req req;
  double t1;
  size_t alignment = sysconf(_SC_PAGESIZE);
  posix_memalign(&buf, alignment, MAX_MSG + alignment);

  if (rank == 0) {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      memset(buf, 'a', size);

      if (size > LARGE) {
        total = TOTAL_LARGE;
        skip = SKIP_LARGE;
      }

      for (int i = 0; i < total + skip; i++) {
        if (i == skip) t1 = wtime();
        meta = i;
        while (lc_sendm(buf, size, 1 - rank, meta, ep_q) != LC_OK)
          lc_progress(0);

        lc_req* req_ptr;
        while (lc_cq_pop(ep_q, &req_ptr) != LC_OK) lc_progress(0);
        assert(req_ptr->meta == i);
        lc_cq_reqfree(ep_q, req_ptr);
      }

      t1 = 1e6 * (wtime() - t1) / total / 2;
      printf("%10.d %10.3f\n", size, t1);
    }
  } else {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      memset(buf, 'a', size);
      if (size > LARGE) {
        total = TOTAL_LARGE;
        skip = SKIP_LARGE;
      }

      for (int i = 0; i < total + skip; i++) {
        lc_req* req_ptr;
        while (lc_cq_pop(ep_q, &req_ptr) != LC_OK) lc_progress(0);
        assert(req_ptr->meta == i);
        lc_cq_reqfree(ep_q, req_ptr);

        meta = i;
        while (lc_sendm(buf, size, 1 - rank, meta, ep_q) != LC_OK)
          lc_progress(0);
      }
    }
  }
  lc_finalize();
}
