#include "lci.h"
#include "include/lci_priv.h"

LCI_error_t LCI_CQ_create(uint32_t length, LCI_CQ_t* cq) {
  struct LCI_CQ_s** cq_s = (struct LCI_CQ_s**) cq;
  lc_cq_create(cq_s);
  return LCI_OK;
}

LCI_error_t LCI_CQ_free(LCI_CQ_t* cq) {
  lc_cq_free((lc_cq*) *cq);
  return LCI_OK;
}

LCI_error_t LCI_CQ_dequeue(LCI_CQ_t* cq, LCI_request_t** req) {
  void* ptr = lc_cq_pop((lc_cq*) *cq);
  if (ptr == NULL) return LCI_ERR_RETRY;
  *req = (LCI_request_t*) ptr;
  return LCI_OK;
}

LCI_error_t LCI_CQ_mul_dequeue(LCI_CQ_t *cq, LCI_request_t requests[], uint8_t count) {
  return LCI_OK;
}


