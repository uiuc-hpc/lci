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
  LCI_endpoint_t ep = LCI_UR_ENDPOINT; // we can directly use the default ep

  int rank = LCI_RANK;
  int peer_rank = LCI_RANK;
  LCI_tag_t tag = 99;

  LCI_comp_t cq;
  LCI_queue_create(0, &cq);

  LCI_mbuffer_t mbuffer;
  LCI_request_t request;
  LCI_barrier();

  for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
    for (int i = 0; i < TOTAL; i++) {
      LCI_mbuffer_alloc(0, &mbuffer);
      write_buffer(mbuffer.address, size, 's');
      mbuffer.length = size;
      LCI_sendmn(ep, mbuffer, peer_rank, tag);

      LCI_recvmn(ep, peer_rank, tag, cq, NULL);
      while (LCI_queue_pop(cq, &request) == LCI_ERR_RETRY)
        LCI_progress(0, 1);
      assert(request.data.mbuffer.length == size);
      check_buffer(request.data.mbuffer.address, size, 's');
      LCI_mbuffer_free(0, request.data.mbuffer);
    }
  }

  LCI_close();
  return 0;
}
