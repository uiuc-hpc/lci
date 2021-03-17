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
  LCI_plist_set_comp_type(plist, LCI_PORT_MESSAGE, LCI_COMPLETION_ONE2ONEL);
  LCI_endpoint_t ep;
  LCI_endpoint_init(&ep, 0, plist);

  int src_rank = LCI_RANK;
  int dst_rank = LCI_RANK;
  LCI_tag_t tag = 99;

  LCI_syncl_t sync;
  LCI_sync_create(&sync);

  double t1 = 0;
  LCI_short_t src = 158;
  LCI_short_t dst;

  for (int i = 0; i < total; i++) {
    LCI_sends(ep, src, src_rank, tag);
    LCI_one2one_set_empty(&sync);
    LCI_recvs(ep, dst_rank, tag, &sync, NULL);
    while (LCI_one2one_test_empty(&sync))
      LCI_progress(0, 1);
    dst = sync.request.data.immediate;
    assert(dst == 158);
  }
  LCI_close();
}
