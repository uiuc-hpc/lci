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
  LCI_endpoint_t ep;
  LCI_PL_t prop;
  LCI_PL_create(&prop);
  LCI_MT_t mt;
  LCI_MT_init(&mt, 0);
  LCI_PL_set_MT(prop,&mt);
  LCI_endpoint_create(0, prop, &ep);

  int rank = LCI_RANK;
  int tag = 99;

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
      LCI_sendi(*(LCI_short_t*) src_buf, rank, tag, ep);
      LCI_one2one_set_empty(&sync);
      LCI_recvi(dst_buf, rank, tag, ep, &sync);
      while (LCI_one2one_test_empty(&sync))
        LCI_progress(0, 1);
      if (i == 0) {
        for (int j = 0; j < size; j++)
          assert(((char*) src_buf)[j] == 'a' && ((char*)dst_buf)[j] == 'a');
      }
    }
  }
  LCI_close();
}
