#include "lci.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "comm_exp.h"

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

  LCI_syncl_t sync_send, sync_recv;

  double t1 = 0;
  size_t alignment = sysconf(_SC_PAGESIZE);
  void* src_buf = 0;
  void* dst_buf = 0;
  posix_memalign(&src_buf, alignment, MAX_MSG);
  posix_memalign(&dst_buf, alignment, MAX_MSG);
  LCI_segment_t src_seg, dst_seg;
  LCI_memory_register(0, src_buf, MAX_MSG, &src_seg);
  LCI_memory_register(0, dst_buf, MAX_MSG, &dst_seg);

  for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
    memset(src_buf, 'a', size);
    memset(dst_buf, 'b', size);

    if (size > LARGE) { total = TOTAL_LARGE; }

    for (int i = 0; i < total; i++) {
      LCI_one2one_set_empty(&sync_send);
      LCI_one2one_set_empty(&sync_recv);
      LCI_recvd(dst_buf, size, rank, tag, ep, &sync_recv);

      while (LCI_sendd(src_buf, size, rank, tag, ep, &sync_send) != LCI_OK)
        LCI_progress(0, 1);
      while (LCI_one2one_test_empty(&sync_send))
        LCI_progress(0, 1);
      LCI_one2one_set_empty(&sync_send);

      while (LCI_one2one_test_empty(&sync_recv))
        LCI_progress(0, 1);
      if (i == 0) {
        for (int j = 0; j < size; j++)
          assert(((char*) src_buf)[j] == 'a' && ((char*)dst_buf)[j] == 'a');
      }
    }
  }
  LCI_memory_deregister(0, &src_seg);
  LCI_memory_deregister(0, &dst_seg);
  assert(src_seg == NULL);
  assert(dst_seg == NULL);
  LCI_close();
}
