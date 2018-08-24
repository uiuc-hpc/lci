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

int main(int argc, char** args) {
  lc_ep ep;
  lc_init(1, EP_AR_EXPL, EP_CE_FLAG, &ep);
  int rank = 0;
  lc_get_proc_num(&rank);
  lc_meta tag = {99};

  lc_req req;
  double t1;
  size_t alignment = sysconf(_SC_PAGESIZE);
  void* src_buf = malloc(MAX_MSG + alignment);
  void* dst_buf = malloc(MAX_MSG + alignment);
  src_buf = (void*)(((uintptr_t) src_buf + alignment - 1) / alignment * alignment);
  dst_buf = (void*)(((uintptr_t) dst_buf + alignment - 1) / alignment * alignment);

  if (rank == 0) {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      memset(src_buf, 'a', size);
      memset(dst_buf, 'b', size);

      if (size > LARGE) { total = TOTAL_LARGE; skip = SKIP_LARGE; }

      for (int i = 0; i < total + skip; i++) {
        if (i == skip) t1 = wtime();
        req.flag = 0;
        while (lc_sendm(ep, 1-rank, src_buf, size, tag, &req) != LC_OK)
          lc_progress_t(0);
        while (req.flag == 0)
          lc_progress_t(0);

        req.flag = 0;
        while (lc_recvm(ep, 1-rank, dst_buf, size, tag, &req) != LC_OK)
          lc_progress_t(0);
        while (req.flag == 0)
          lc_progress_t(0);
        if (i == 0) {
          for (int j = 0; j < size; j++)
            assert(((char*) src_buf)[j] == 'a' && ((char*)dst_buf)[j] == 'a');
        }
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
        while (lc_recvm(ep, 1-rank, dst_buf, size, tag, &req) != LC_OK)
          lc_progress_t(0);
        while (req.flag == 0)
          lc_progress_t(0);

        req.flag = 0;
        while (lc_sendm(ep, 1-rank, src_buf, size, tag, &req) != LC_OK)
          lc_progress_t(0);
        while (req.flag == 0)
          lc_progress_t(0);

      }
    }
  }
  lc_finalize();
}
