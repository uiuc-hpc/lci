#include "lci.h"
#include "comm_exp.h"

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#undef MAX_MSG
#define MAX_MSG (8 * 1024)

int main(int argc, char** args) {
  LCI_initialize(&argc, &args);
  LCI_endpoint_t ep;
  LCI_CQ_t cq;
  LCI_CQ_create(0, &cq);

  LCI_PL_t prop;
  LCI_PL_create(&prop);
  LCI_PL_set_cq(&cq, &prop);
  LCI_PL_set_completion(LCI_PORT_MESSAGE, LCI_COMPLETION_QUEUE, &prop);

  LCI_endpoint_create(0, prop, &ep);
  int rank = LCI_RANK;
  LCI_PM_barrier();

  LCI_syncl_t sync;
  LCI_sync_create(&sync);

  LCI_PM_barrier();

  double t1;
  LCI_bdata_t p;
  LCI_buffer_get(ep, &p);
  LCI_request_t* req_ptr;

  for (int size = 1; size <= MAX_MSG; size <<= 1) {
    for (int i = 0; i < TOTAL+SKIP; i++) {
      if (i == SKIP)
        t1 = wtime();
      if (rank == 0) {
        LCI_one2one_set_empty(&sync);
        LCI_putb(p, size, 1-rank, 99, ep, &sync);
        while (LCI_one2one_test_empty(&sync)) {
          LCI_progress(0, 1);
        }
        while (LCI_CQ_dequeue(&cq, &req_ptr) == LCI_ERR_RETRY)
          LCI_progress(0, 1);
        LCI_buffer_free(ep, req_ptr->data.buffered);
      } else {
        while (LCI_CQ_dequeue(&cq, &req_ptr) == LCI_ERR_RETRY)
          LCI_progress(0, 1);
        LCI_buffer_free(ep, req_ptr->data.buffered);
        LCI_one2one_set_empty(&sync);
        LCI_putb(p, size, 1-rank, 99, ep, &sync);
        while (LCI_one2one_test_empty(&sync)) {
          LCI_progress(0, 1);
        }
      }
    }

    if (rank == 0) {
      t1 = 1e6 * (wtime() - t1) / TOTAL / 2;
      printf("%10.d %10.3f\n", size, t1);
    }
  }

  LCI_finalize();
}