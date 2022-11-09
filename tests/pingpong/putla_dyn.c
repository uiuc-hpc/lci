#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "lci.h"
#include "comm_exp.h"

int total = 10;

int main(int argc, char** args)
{
  LCI_initialize();
  LCI_endpoint_t ep = LCI_UR_ENDPOINT;  // we can directly use the default ep
  LCI_comp_t send_cq;
  LCI_queue_create(LCI_UR_DEVICE, &send_cq);

  int rank = LCI_RANK;
  int peer_rank = ((1 - LCI_RANK % 2) + LCI_RANK / 2 * 2) % LCI_NUM_PROCESSES;
  LCI_tag_t tag = 99;

  size_t alignment = sysconf(_SC_PAGESIZE);
  LCI_lbuffer_t src_buf;
  posix_memalign(&src_buf.address, alignment, MAX_MSG);
  src_buf.segment = LCI_SEGMENT_ALL;
  LCI_request_t request;
  LCI_barrier();

  if (rank % 2 == 0) {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      printf("Testing message size %d...\n", size);
      fflush(stdout);
      src_buf.length = size;
      for (int i = 0; i < total; i++) {
        write_buffer(src_buf.address, size, 's');
        while (LCI_putla(ep, src_buf, send_cq, peer_rank, tag,
                         LCI_DEFAULT_COMP_REMOTE, NULL) == LCI_ERR_RETRY)
          LCI_progress(LCI_UR_DEVICE);
        while (LCI_queue_pop(send_cq, &request) == LCI_ERR_RETRY)
          LCI_progress(LCI_UR_DEVICE);
        while (LCI_queue_pop(LCI_UR_CQ, &request) == LCI_ERR_RETRY)
          LCI_progress(LCI_UR_DEVICE);
        check_buffer(request.data.lbuffer.address, size, 's');
        LCI_lbuffer_free(request.data.lbuffer);
      }
    }
  } else {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      src_buf.length = size;
      for (int i = 0; i < total; i++) {
        write_buffer(src_buf.address, size, 's');
        while (LCI_queue_pop(LCI_UR_CQ, &request) == LCI_ERR_RETRY)
          LCI_progress(LCI_UR_DEVICE);
        check_buffer(request.data.lbuffer.address, size, 's');
        LCI_lbuffer_free(request.data.lbuffer);
        while (LCI_putla(ep, src_buf, send_cq, peer_rank, tag,
                         LCI_DEFAULT_COMP_REMOTE, NULL) == LCI_ERR_RETRY)
          LCI_progress(LCI_UR_DEVICE);
        while (LCI_queue_pop(send_cq, &request) == LCI_ERR_RETRY)
          LCI_progress(LCI_UR_DEVICE);
      }
    }
  }
  LCI_finalize();
  return 0;
}
