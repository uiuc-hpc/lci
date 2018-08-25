#include "lc.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "comm_exp.h"

#undef MAX_MSG
#define MAX_MSG (8*1024)

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
  lc_init(1, LC_ALLOC_CQ, &ep[0]);
  lc_ep_dup(0, LC_ALLOC_CQ, ep[0], &ep[1]);

  lc_ep_set_alloc(ep[0], no_alloc, no_free, NULL);
  lc_ep_set_alloc(ep[1], no_alloc, no_free, NULL);

  int rank = 0;
  lc_get_proc_num(&rank);
  lc_meta meta = {99};

  lc_req req;
  double t1;
  size_t alignment = sysconf(_SC_PAGESIZE);
  buf = malloc(MAX_MSG);
  buf = (void*)(((uintptr_t) buf + alignment - 1) / alignment * alignment);

  if (rank == 0) {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      memset(buf, 'a', size);

      if (size > LARGE) { total = TOTAL_LARGE; skip = SKIP_LARGE; }

      for (int i = 0; i < total + skip; i++) {
        if (i == skip) t1 = wtime();
        meta = i;
        while (lc_putmd(buf, size, 1-rank, meta, ep[i&1]) != LC_OK)
          lc_progress_q(0);

        lc_req* req_ptr;
        while (lc_cq_pop(ep[i&1], &req_ptr) != LC_OK)
          lc_progress_q(0);
        assert(req_ptr->meta == i);
        lc_cq_reqfree(ep[i&1], req_ptr);
      }

      t1 = 1e6 * (wtime() - t1) / total / 2;
      printf("%10.d %10.3f\n", size, t1);
    }
  } else {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      memset(buf, 'a', size);
      if (size > LARGE) { total = TOTAL_LARGE; skip = SKIP_LARGE; }

      for (int i = 0; i < total + skip; i++) {
        lc_req* req_ptr;
        while (lc_cq_pop(ep[i&1], &req_ptr) != LC_OK)
          lc_progress_q(0);
        assert(req_ptr->meta == i);
        lc_cq_reqfree(ep[i&1], req_ptr);

        meta = i;
        while (lc_putmd(buf, size, 1-rank, meta, ep[i&1]) != LC_OK)
          lc_progress_q(0);
      }
    }
  }
  lc_finalize();
}
