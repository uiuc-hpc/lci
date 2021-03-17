#include "lci.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "comm_exp.h"

#undef MAX_MSG
#define MAX_MSG 8

int total = TOTAL;

int main(int argc, char** args) {
  LCI_open();
  LCI_endpoint_t ep = LCI_UR_ENDPOINT;

  int src_rank = LCI_RANK;
  int dst_rank = LCI_RANK;
  LCI_tag_t tag = 99;

  LCI_syncl_t sync;
  LCI_sync_create(&sync);

  double t1 = 0;
  size_t alignment = sysconf(_SC_PAGESIZE);
  void* src_buf = 0;
  void* dst_buf = 0;
  posix_memalign(&src_buf, alignment, MAX_MSG);
  posix_memalign(&dst_buf, alignment, MAX_MSG);

  for (int size = sizeof(LCI_short_t); size <= sizeof(LCI_short_t); size <<= 1) {
    memset(src_buf, 'a', size);
    memset(dst_buf, 'b', size);

    if (size > LARGE) { total = TOTAL_LARGE; }

    for (int i = 0; i < total; i++) {
      LCI_sends(ep, *(LCI_short_t*)src_buf, src_rank, tag);
      LCI_one2one_set_empty(&sync);
      LCI_recvs(ep, dst_rank, tag, &sync, NULL);
      while (LCI_one2one_test_empty(&sync))
        LCI_progress(0, 1);
      for (int j = 0; j < size; j++)
        assert(((char*) src_buf)[j] == 'a' && ((char*)dst_buf)[j] == 'a');
    }
  }
  LCI_close();
}
