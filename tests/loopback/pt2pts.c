#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "lci.h"
#include "comm_exp.h"

int total = TOTAL;

int main(int argc, char** args) {
  LCI_open();
  LCI_plist_t plist;
  LCI_plist_create(&plist);
  LCI_plist_set_comp_type(plist, LCI_PORT_MESSAGE, LCI_COMPLETION_SYNC);
  LCI_endpoint_t ep;
  LCI_endpoint_init(&ep, LCI_UR_DEVICE, plist);
  LCI_plist_free(&plist);

  int rank = LCI_RANK;
  int peer_rank = LCI_RANK;
  LCI_tag_t tag = 99;

  LCI_comp_t sync;
  LCI_sync_create(LCI_UR_DEVICE, LCI_SYNC_SIMPLE, &sync);

  LCI_short_t src = 158;
  LCI_barrier();

  for (int i = 0; i < total; i++) {
    while (LCI_sends(ep, src, peer_rank, tag) == LCI_ERR_RETRY)
      LCI_progress(LCI_UR_DEVICE);
    LCI_recvs(ep, rank, tag, sync, NULL);
    LCI_request_t request;
    while (LCI_sync_test(sync, &request) == LCI_ERR_RETRY)
      LCI_progress(LCI_UR_DEVICE);
    assert(request.data.immediate == 158);
  }
  LCI_sync_free(&sync);
  LCI_close();
  return 0;
}
