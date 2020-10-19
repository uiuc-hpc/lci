#include "lci.h"
#include "include/lci_priv.h"

LCI_error_t LCI_CQ_init(LCI_comp_t* cq, uint32_t length)
{
  struct LCI_CQ_s** cq_s = (struct LCI_CQ_s**) cq;
  lc_cq_create(cq_s);
  return LCI_OK;
}

LCI_error_t LCI_CQ_free(LCI_comp_t cq) {
  lc_cq_free((lc_cq*) cq);
  return LCI_OK;
}

LCI_error_t LCI_dequeue(LCI_comp_t cq, LCI_request_t** req) {
  void* ptr = lc_cq_pop((lc_cq*) cq);
  if (ptr == NULL) return LCI_ERR_RETRY;
  *req = (LCI_request_t*) ptr;
  return LCI_OK;
}

LCI_error_t LCI_wait_dequeue(LCI_comp_t cq, LCI_request_t** req) {
  void* ptr = NULL;
  while (ptr == NULL) {
    ptr = lc_cq_pop((lc_cq*) cq);
  }
  *req = (LCI_request_t*) ptr;
  return LCI_OK;
}

LCI_error_t LCI_mult_dequeue(LCI_comp_t cq ,
                             LCI_request_t requests[] ,
                             uint32_t request_count ,
                             uint32_t *return_count ) {
  uint32_t count = 0;
  void* ptr;
  while (count < request_count) {
    ptr = lc_cq_pop((lc_cq*)cq);
    if (ptr != NULL) {
      requests[count] = *(LCI_request_t*)ptr;
      ++count;
    } else {
      break;
    }
  }
  *return_count = count;
  return LCI_OK;
}


