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
  LCI_endpoint_t ep = LCI_UR_ENDPOINT;  // we can directly use the default ep

  int rank = LCI_RANK;
  int peer_rank = ((1 - LCI_RANK % 2) + LCI_RANK / 2 * 2) % LCI_NUM_PROCESSES;
  LCI_tag_t tag = 99;

  LCI_comp_t cq;
  LCI_queue_create(0, &cq);

  LCI_mbuffer_t mbuffer;
  LCI_request_t request;
  LCI_barrier();

  if (rank % 2 == 0) {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      for (int i = 0; i < TOTAL; i++) {
        LCI_mbuffer_alloc(LCI_UR_DEVICE, &mbuffer);
        write_buffer(mbuffer.address, size, 's');
        mbuffer.length = size;
        while (LCI_sendmn(ep, mbuffer, peer_rank, tag) == LCI_ERR_RETRY)
          LCI_progress(LCI_UR_DEVICE);

        LCI_recvmn(ep, peer_rank, tag, cq, NULL);
        while (LCI_queue_pop(cq, &request) == LCI_ERR_RETRY)
          LCI_progress(LCI_UR_DEVICE);
        assert(request.data.mbuffer.length == size);
        check_buffer(request.data.mbuffer.address, size, 's');
        LCI_mbuffer_free(request.data.mbuffer);
      }
    }
  } else {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      for (int i = 0; i < TOTAL; i++) {
        LCI_recvmn(ep, peer_rank, tag, cq, NULL);
        while (LCI_queue_pop(cq, &request) == LCI_ERR_RETRY)
          LCI_progress(LCI_UR_DEVICE);
        assert(request.data.mbuffer.length == size);
        check_buffer(request.data.mbuffer.address, size, 's');
        LCI_mbuffer_free(request.data.mbuffer);

        LCI_mbuffer_alloc(LCI_UR_DEVICE, &mbuffer);
        write_buffer(mbuffer.address, size, 's');
        mbuffer.length = size;
        while (LCI_sendmn(ep, mbuffer, peer_rank, tag) == LCI_ERR_RETRY)
          LCI_progress(LCI_UR_DEVICE);
      }
    }
  }
  LCI_queue_free(&cq);
  LCI_finalize();
  return 0;
}
