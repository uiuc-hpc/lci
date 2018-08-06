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
  lc_dev dev;

  lc_dev_open(&dev);
  lc_ep_open(dev, EP_TYPE_TAG, &ep);
  int rank = 0;
  lc_get_proc_num(&rank);
  lc_ep_query(dev, 1-rank, 0, &rep);
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
        while (lc_send_tag(ep, rep, src_buf, size, tag, &req) != LC_OK)
          lc_progress_t(dev);
        while (req.flag == 0)
          lc_progress_t(dev);

        req.flag = 0;
        while (lc_recv_tag(ep, rep, dst_buf, size, tag, &req) != LC_OK)
          lc_progress_t(dev);
        while (req.flag == 0)
          lc_progress_t(dev);
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
        while (lc_recv_tag(ep, rep, dst_buf, size, tag, &req) != LC_OK)
          lc_progress_t(dev);
        while (req.flag == 0)
          lc_progress_t(dev);

        req.flag = 0;
        while (lc_send_tag(ep, rep, src_buf, size, tag, &req) != LC_OK)
          lc_progress_t(dev);
        while (req.flag == 0)
          lc_progress_t(dev);

      }
    }
  }
  lc_finalize();
}
