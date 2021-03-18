#include "lci.h"
#include "comm_exp.h"

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#undef MAX_MSG
#define MAX_MSG (8 * 1024)

int main(int argc, char** args) {
  LCI_open();
  LCI_endpoint_t ep;
  LCI_comp_t cq;
  LCI_queue_create(0, &cq);

  LCI_plist_t prop;
  LCI_plist_create(&prop);
  LCI_plist_set_CQ(prop,&cq);
  LCI_plist_set_completion(prop,LCI_PORT_MESSAGE, LCI_COMPLETION_QUEUE);

  LCI_endpoint_init(&ep, 0, prop);
  int rank = LCI_RANK;
  LCI_barrier();

  LCI_syncl_t sync;
  LCI_sync_create(&sync);

  LCI_barrier();
  LCI_mbuffer_t p;
  LCI_mbuffer_alloc(0, &p);
  LCI_request_t* req_ptr;

  for (int size = 1; size <= MAX_MSG; size <<= 1) {
    for (int i = 0; i < TOTAL; i++) {
      if (rank == 0) {
        LCI_one2one_set_empty(&sync);
        LCI_putb(p, size, 1-rank, 99, ep, &sync);
        while (LCI_one2one_test_empty(&sync)) {
          LCI_progress(0, 1);
        }
        while (LCI_queue_pop(cq, &req_ptr) == LCI_ERR_RETRY)
          LCI_progress(0, 1);
        LCI_mbuffer_free(0, req_ptr->data.buffer.start);
      } else {
        while (LCI_queue_pop(cq, &req_ptr) == LCI_ERR_RETRY)
          LCI_progress(0, 1);
        LCI_mbuffer_free(0, req_ptr->data.buffer.start);
        LCI_one2one_set_empty(&sync);
        LCI_putb(p, size, 1-rank, 99, ep, &sync);
        while (LCI_one2one_test_empty(&sync)) {
          LCI_progress(0, 1);
        }
      }
    }
  }

  LCI_close();
  return 0;
}
