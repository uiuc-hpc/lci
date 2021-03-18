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
  LCI_plist_set_comp_type(plist, LCI_PORT_MESSAGE, LCI_COMPLETION_ONE2ONEL);
  LCI_endpoint_t ep;
  LCI_endpoint_init(&ep, 0, plist);

  int rank = LCI_RANK;
  int peer_rank = LCI_RANK;
  LCI_tag_t tag = 99;

  LCI_syncl_t sync;
  LCI_sync_create(&sync);

  LCI_short_t src = 158;
  LCI_barrier();

  for (int i = 0; i < total; i++) {
    LCI_sends(ep, src, peer_rank, tag);
    LCI_one2one_set_empty(&sync);
    LCI_recvs(ep, rank, tag, &sync, NULL);
    while (LCI_one2one_test_empty(&sync))
      LCI_progress(0, 1);
    assert(sync.request.data.immediate == 158);
  }
  LCI_close();
  return 0;
}
