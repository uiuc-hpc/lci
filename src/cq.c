#include "lci.h"
#include "lcii.h"

LCI_error_t LCI_queue_create(uint8_t device, LCI_comp_t* cq)
{
  lc_cq* cq_ptr;
  lc_cq_create(&cq_ptr);
  *cq = cq_ptr;
  return LCI_OK;
}

LCI_error_t LCI_queue_free(LCI_comp_t* cq) {
  lc_cq_free((lc_cq*)*cq);
  *cq = NULL;
  return LCI_OK;
}

LCI_error_t LCI_queue_pop(LCI_comp_t cq, LCI_request_t* request) {
  LCII_context_t *ctx = lc_cq_pop((lc_cq*) cq);
  if (ctx == NULL) return LCI_ERR_RETRY;
  *request = LCII_ctx2req(ctx);
  return LCI_OK;
}

LCI_error_t LCI_queue_wait(LCI_comp_t cq, LCI_request_t* request) {
  LCII_context_t *ctx = NULL;
  while (ctx == NULL) {
    ctx = lc_cq_pop((lc_cq*) cq);
  }
  *request = LCII_ctx2req(ctx);
  return LCI_OK;
}

LCI_error_t LCI_queue_pop_multiple(LCI_comp_t cq, uint32_t request_count,
                                   LCI_request_t* requests,
                                   uint32_t* return_count)
{
  int count = 0;
  LCII_context_t* ctx;
  while (count < request_count) {
    ctx = lc_cq_pop((lc_cq*)cq);
    if (ctx != NULL) {
      requests[count] = LCII_ctx2req(ctx);
      ++count;
    } else {
      break;
    }
  }
  *return_count = count;
  return LCI_OK;
}

LCI_error_t LCI_queue_wait_multiple(LCI_comp_t cq, uint32_t request_count,
                                   LCI_request_t* requests)
{
  int count = 0;
  LCII_context_t* ctx;
  while (count < request_count) {
    ctx = lc_cq_pop((lc_cq*)cq);
    if (ctx != NULL) {
      requests[count] = LCII_ctx2req(ctx);
      ++count;
    } else {
      continue;
    }
  }
  return LCI_OK;
}


