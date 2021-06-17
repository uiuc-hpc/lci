#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "lci.h"
#include "comm_exp.h"

int total = 10;

int main(int argc, char** args) {
  LCI_open();
  LCI_endpoint_t ep = LCI_UR_ENDPOINT; // we can directly use the default ep

  int rank = LCI_RANK;
  int peer_rank = ((1 - LCI_RANK % 2) + LCI_RANK / 2 * 2) % LCI_NUM_PROCESSES;
  LCI_tag_t tag = 99;

  size_t alignment = sysconf(_SC_PAGESIZE);
  LCI_lbuffer_t src_buf;
  LCI_lbuffer_alloc(LCI_UR_DEVICE, MAX_MSG, &src_buf);
  LCI_request_t request;
  LCI_barrier();

  if (rank % 2 == 0) {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      printf("Testing message size %d...\n", size);
      src_buf.length = size;
      for (int i = 0; i < total; i++) {
        write_buffer(src_buf.address, size, 's');
        while (LCI_putla(ep, src_buf, LCI_UR_CQ, peer_rank, tag, LCI_UR_CQ_REMOTE, NULL) == LCI_ERR_RETRY)
          LCI_progress(LCI_UR_DEVICE);
        while (LCI_queue_pop(LCI_UR_CQ, &request) == LCI_ERR_RETRY)
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
        while (LCI_putla(ep, src_buf, LCI_UR_CQ, peer_rank, tag, LCI_UR_CQ_REMOTE, NULL) == LCI_ERR_RETRY)
          LCI_progress(LCI_UR_DEVICE);
        while (LCI_queue_pop(LCI_UR_CQ, &request) == LCI_ERR_RETRY)
          LCI_progress(LCI_UR_DEVICE);
      }
    }
  }
  LCI_lbuffer_free(src_buf);
  LCI_close();
  return 0;
}
