#include "lci.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "comm_exp.h"

int total = TOTAL_LARGE;

int main(int argc, char** args)
{
  LCI_initialize();
  LCI_plist_t plist;
  LCI_plist_create(&plist);
  LCI_plist_set_comp_type(plist, LCI_PORT_COMMAND, LCI_COMPLETION_SYNC);
  LCI_plist_set_comp_type(plist, LCI_PORT_MESSAGE, LCI_COMPLETION_SYNC);
  LCI_endpoint_t ep;
  LCI_endpoint_init(&ep, LCI_UR_DEVICE, plist);
  LCI_plist_free(&plist);

  int rank = LCI_RANK;
  int peer_rank = ((1 - LCI_RANK % 2) + LCI_RANK / 2 * 2) % LCI_NUM_PROCESSES;
  LCI_tag_t tag = 99;

  LCI_comp_t sync_send, sync_recv;
  LCI_sync_create(LCI_UR_DEVICE, 1, &sync_send);
  LCI_sync_create(LCI_UR_DEVICE, 1, &sync_recv);

  size_t alignment = sysconf(_SC_PAGESIZE);
  LCI_lbuffer_t src_buf, dst_buf;
  posix_memalign(&src_buf.address, alignment, MAX_MSG);
  posix_memalign(&dst_buf.address, alignment, MAX_MSG);
  src_buf.segment = LCI_SEGMENT_ALL;
  dst_buf.segment = LCI_SEGMENT_ALL;

  if (rank % 2 == 0) {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      printf("Testing message size %d...\n", size);
      fflush(stdout);
      src_buf.length = size;
      dst_buf.length = size;

      if (size > LARGE) {
        total = TOTAL_LARGE;
      }

      for (int i = 0; i < total; i++) {
        write_buffer(src_buf.address, size, 's');
        write_buffer(dst_buf.address, size, 'r');

        while (LCI_sendl(ep, src_buf, peer_rank, tag, sync_send, NULL) !=
               LCI_OK)
          LCI_progress(LCI_UR_DEVICE);
        while (LCI_sync_test(sync_send, NULL) == LCI_ERR_RETRY)
          LCI_progress(LCI_UR_DEVICE);

        while (LCI_recvl(ep, dst_buf, peer_rank, tag, sync_recv, NULL) !=
               LCI_OK)
          LCI_progress(LCI_UR_DEVICE);
        while (LCI_sync_test(sync_recv, NULL) == LCI_ERR_RETRY)
          LCI_progress(LCI_UR_DEVICE);
        check_buffer(dst_buf.address, size, 's');
      }
    }
  } else {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      src_buf.length = size;
      dst_buf.length = size;

      if (size > LARGE) {
        total = TOTAL_LARGE;
      }

      for (int i = 0; i < total; i++) {
        write_buffer(src_buf.address, size, 's');
        write_buffer(dst_buf.address, size, 'r');

        while (LCI_recvl(ep, dst_buf, peer_rank, tag, sync_recv, NULL) !=
               LCI_OK)
          LCI_progress(LCI_UR_DEVICE);
        while (LCI_sync_test(sync_recv, NULL) == LCI_ERR_RETRY)
          LCI_progress(LCI_UR_DEVICE);
        check_buffer(dst_buf.address, size, 's');

        while (LCI_sendl(ep, src_buf, peer_rank, tag, sync_send, NULL) !=
               LCI_OK)
          LCI_progress(LCI_UR_DEVICE);
        while (LCI_sync_test(sync_send, NULL) == LCI_ERR_RETRY)
          LCI_progress(LCI_UR_DEVICE);
      }
    }
  }
  LCI_sync_free(&sync_recv);
  LCI_sync_free(&sync_send);
  LCI_endpoint_free(&ep);
  LCI_finalize();
  return 0;
}
