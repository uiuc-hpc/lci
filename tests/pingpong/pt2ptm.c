#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "lci.h"
#include "comm_exp.h"

#undef MAX_MSG
#define MAX_MSG LCI_MEDIUM_SIZE

int total = TOTAL;

int main(int argc, char** args)
{
  LCI_initialize();
  LCI_plist_t plist;
  LCI_plist_create(&plist);
  LCI_plist_set_comp_type(plist, LCI_PORT_MESSAGE, LCI_COMPLETION_SYNC);
  LCI_endpoint_t ep;
  LCI_endpoint_init(&ep, LCI_UR_DEVICE, plist);
  LCI_plist_free(&plist);

  int rank = LCI_RANK;
  int peer_rank = ((1 - LCI_RANK % 2) + LCI_RANK / 2 * 2) % LCI_NUM_PROCESSES;
  LCI_tag_t tag = 99;

  LCI_comp_t sync;
  LCI_sync_create(LCI_UR_DEVICE, 1, &sync);

  size_t alignment = sysconf(_SC_PAGESIZE);
  LCI_mbuffer_t src_buf, dst_buf;
  posix_memalign(&src_buf.address, alignment, MAX_MSG);
  posix_memalign(&dst_buf.address, alignment, MAX_MSG);

  if (rank % 2 == 0) {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      src_buf.length = size;
      dst_buf.length = size;

      for (int i = 0; i < TOTAL; i++) {
        fflush(stdout);
        write_buffer(src_buf.address, size, 's');
        write_buffer(dst_buf.address, size, 'r');

        while (LCI_sendm(ep, src_buf, peer_rank, tag) == LCI_ERR_RETRY)
          LCI_progress(LCI_UR_DEVICE);

        LCI_recvm(ep, dst_buf, peer_rank, tag, sync, NULL);
        while (LCI_sync_test(sync, NULL) == LCI_ERR_RETRY)
          LCI_progress(LCI_UR_DEVICE);
        check_buffer(dst_buf.address, size, 's');
      }
    }
  } else {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      src_buf.length = size;
      dst_buf.length = size;

      for (int i = 0; i < TOTAL; i++) {
        write_buffer(src_buf.address, size, 's');
        write_buffer(dst_buf.address, size, 'r');

        LCI_recvm(ep, dst_buf, peer_rank, tag, sync, NULL);
        while (LCI_sync_test(sync, NULL) == LCI_ERR_RETRY)
          LCI_progress(LCI_UR_DEVICE);
        check_buffer(dst_buf.address, size, 's');

        while (LCI_sendm(ep, src_buf, peer_rank, tag) == LCI_ERR_RETRY)
          LCI_progress(LCI_UR_DEVICE);
      }
    }
  }
  LCI_finalize();
  return 0;
}
