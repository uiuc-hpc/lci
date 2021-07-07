#include "lcii.h"

LCI_error_t LCI_queue_create(LCI_device_t device, LCI_comp_t* cq)
{
  LCII_cq_t* cq_ptr = LCIU_malloc(sizeof(LCII_cq_t));
  LCM_dq_init(&cq_ptr->dequeue, LCI_DEFAULT_QUEUE_LENGTH);
  LCIU_spinlock_init(&cq_ptr->spinlock);
  *cq = cq_ptr;
  return LCI_OK;
}

LCI_error_t LCI_queue_free(LCI_comp_t* cq) {
  LCII_cq_t* cq_ptr = *cq;
  LCIU_spinlock_fina(&cq_ptr->spinlock);
  LCM_dq_finalize(&cq_ptr->dequeue);
  LCIU_free(cq_ptr);
  *cq = NULL;
  return LCI_OK;
}

LCI_error_t LCI_queue_pop(LCI_comp_t cq, LCI_request_t* request) {
  LCII_cq_t* cq_ptr = cq;
  LCIU_acquire_spinlock(&cq_ptr->spinlock);
  LCII_context_t *ctx = LCM_dq_pop_bot(&cq_ptr->dequeue);
  LCIU_release_spinlock(&cq_ptr->spinlock);
  if (ctx == NULL) return LCI_ERR_RETRY;
  *request = LCII_ctx2req(ctx);
  return LCI_OK;
}

LCI_error_t LCI_queue_wait(LCI_comp_t cq, LCI_request_t* request) {
  LCII_cq_t* cq_ptr = cq;
  LCII_context_t *ctx = NULL;
  while (ctx == NULL) {
    LCIU_acquire_spinlock(&cq_ptr->spinlock);
    ctx = LCM_dq_pop_bot(&cq_ptr->dequeue);
    LCIU_release_spinlock(&cq_ptr->spinlock);
  }
  *request = LCII_ctx2req(ctx);
  return LCI_OK;
}

LCI_error_t LCI_queue_pop_multiple(LCI_comp_t cq, uint32_t request_count,
                                   LCI_request_t* requests,
                                   uint32_t* return_count)
{
  LCII_cq_t* cq_ptr = cq;
  int count = 0;
  LCII_context_t* ctx;
  while (count < request_count) {
    LCIU_acquire_spinlock(&cq_ptr->spinlock);
    ctx = LCM_dq_pop_bot(&cq_ptr->dequeue);
    LCIU_release_spinlock(&cq_ptr->spinlock);
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
  LCII_cq_t* cq_ptr = cq;
  int count = 0;
  LCII_context_t* ctx;
  while (count < request_count) {
    LCIU_acquire_spinlock(&cq_ptr->spinlock);
    ctx = LCM_dq_pop_bot(&cq_ptr->dequeue);
    LCIU_release_spinlock(&cq_ptr->spinlock);
    if (ctx != NULL) {
      requests[count] = LCII_ctx2req(ctx);
      ++count;
    } else {
      continue;
    }
  }
  return LCI_OK;
}

LCI_error_t LCI_queue_len(LCI_comp_t cq, size_t *len) {
  LCII_cq_t* cq_ptr = cq;
  *len = LCM_dq_size(cq_ptr->dequeue);
}