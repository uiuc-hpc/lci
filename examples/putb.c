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

  LCI_plist_t plist;
  LCI_plist_create(&plist);
  LCI_plist_set_CQ(plist,&cq);
  LCI_plist_set_completion(plist,LCI_PORT_MESSAGE, LCI_COMPLETION_QUEUE);

  LCI_endpoint_init(&ep, LCI_UR_DEVICE, plist);
  int rank = LCI_RANK;
  LCI_barrier();

  LCI_comp_t sync;
  LCI_sync_create(&sync);

  LCI_barrier();

  double t1;
  LCI_mbuffer_t p;
  LCI_mbuffer_alloc(LCI_UR_DEVICE, &p);
  LCI_request_t* req_ptr;

  for (int size = 1; size <= MAX_MSG; size <<= 1) {
    for (int i = 0; i < TOTAL+SKIP; i++) {
      if (i == SKIP)
        t1 = wtime();
      if (rank == 0) {
        LCI_one2one_set_empty(&sync);
        LCI_putb(p, size, 1-rank, 99, ep, &sync);
        while (LCI_one2one_test_empty(&sync)) {
          LCI_progress(LCI_UR_DEVICE);
        }
        while (LCI_queue_pop(cq, &req_ptr) == LCI_ERR_RETRY)
          LCI_progress(LCI_UR_DEVICE);
        LCI_mbuffer_free(0, req_ptr->data.buffer.start);
      } else {
        while (LCI_queue_pop(cq, &req_ptr) == LCI_ERR_RETRY)
          LCI_progress(LCI_UR_DEVICE);
        LCI_mbuffer_free(0, req_ptr->data.buffer.start);
        LCI_one2one_set_empty(&sync);
        LCI_putb(p, size, 1-rank, 99, ep, &sync);
        while (LCI_one2one_test_empty(&sync)) {
          LCI_progress(LCI_UR_DEVICE);
        }
      }
    }

    if (rank == 0) {
      t1 = 1e6 * (wtime() - t1) / TOTAL / 2;
      printf("%10.d %10.3f\n", size, t1);
    }
  }

  LCI_close();
}
