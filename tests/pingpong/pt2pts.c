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
  LCI_plist_t plist;
  LCI_plist_create(&plist);
  LCI_plist_set_comp_type(plist, LCI_PORT_MESSAGE, LCI_COMPLETION_SYNC);
  LCI_endpoint_t ep;
  LCI_endpoint_init(&ep, 0, plist);
  LCI_plist_free(&plist);

  int rank = LCI_RANK;
  int peer_rank = ((1 - LCI_RANK % 2) + LCI_RANK / 2 * 2) % LCI_NUM_PROCESSES;
  LCI_tag_t tag = 99;

  LCI_comp_t sync;
  LCI_sync_create(0, LCI_SYNC_SIMPLE, &sync);

  LCI_short_t src = rank;
  LCI_barrier();

  if (rank % 2 == 0) {
    for (int i = 0; i < total; i++) {
      LCI_sends(ep, src, peer_rank, tag);

      LCI_recvs(ep, peer_rank, tag, sync, NULL);
      LCI_request_t request;
      while (LCI_sync_test(sync, &request) == LCI_ERR_RETRY)
        LCI_progress(0, 1);
      assert(request.data.immediate == peer_rank);
    }
  } else {
    for (int i = 0; i < total; i++) {

      LCI_recvs(ep, peer_rank, tag, sync, NULL);
      LCI_request_t request;
      while (LCI_sync_test(sync, &request) == LCI_ERR_RETRY)
        LCI_progress(0, 1);
      assert(request.data.immediate == peer_rank);
      LCI_sends(ep, src, peer_rank, tag);
    }
  }
  LCI_close();
  return 0;
}
