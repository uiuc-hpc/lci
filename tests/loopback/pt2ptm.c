#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "lci.h"
#include "comm_exp.h"

#undef MAX_MSG
#define MAX_MSG (8 * 1024)

int main(int argc, char** args) {
  LCI_open();
  LCI_plist_t plist;
  LCI_plist_create(&plist);
  LCI_plist_set_comp_type(plist, LCI_PORT_MESSAGE, LCI_COMPLETION_ONE2ONEL);
  LCI_endpoint_t ep;
  LCI_endpoint_init(&ep, 0, plist);

  int rank = LCI_RANK;
  int peer_rank = LCI_RANK;
  LCI_tag_t tag = 99;

  LCI_syncl_t sync;
  LCI_sync_create(&sync);

  size_t alignment = sysconf(_SC_PAGESIZE);
  LCI_mbuffer_t src_buf, dst_buf;
  posix_memalign(&src_buf.address, alignment, MAX_MSG);
  posix_memalign(&dst_buf.address, alignment, MAX_MSG);

  for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
    src_buf.length = size;
    dst_buf.length = size;

    for (int i = 0; i < TOTAL; i++) {
      write_buffer(src_buf.address, size, 's');
      write_buffer(dst_buf.address, size, 'r');
      while (LCI_sendm(ep, src_buf, peer_rank, tag) != LCI_OK)
        LCI_progress(0, 1);

      LCI_one2one_set_empty(&sync);
      LCI_recvm(ep, dst_buf, peer_rank, tag, &sync, NULL);
      while (LCI_one2one_test_empty(&sync))
        LCI_progress(0, 1);
      check_buffer(dst_buf.address, size, 's');
    }
  }
  LCI_close();
  return 0;
}
