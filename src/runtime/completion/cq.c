#include "runtime/lcii.h"

LCI_error_t LCI_queue_create(LCI_device_t device, LCI_comp_t* cq)
{
  LCII_cq_t* cq_ptr = LCIU_malloc(sizeof(LCII_cq_t));
#ifdef LCI_USE_MUTEX_CQ
  LCM_dq_init(&cq_ptr->dequeue, LCI_DEFAULT_QUEUE_LENGTH);
  LCIU_spinlock_init(&cq_ptr->spinlock);
#else
  LCM_aqueue_init(cq_ptr, LCI_DEFAULT_QUEUE_LENGTH);
#endif
  *cq = cq_ptr;
  return LCI_OK;
}

LCI_error_t LCI_queue_free(LCI_comp_t* cq)
{
  LCII_cq_t* cq_ptr = *cq;
#ifdef LCI_USE_MUTEX_CQ
  LCIU_spinlock_fina(&cq_ptr->spinlock);
  LCM_dq_finalize(&cq_ptr->dequeue);
#else
  LCM_aqueue_fina(cq_ptr);
#endif
  LCIU_free(cq_ptr);
  *cq = NULL;
  return LCI_OK;
}

LCI_error_t LCI_queue_pop(LCI_comp_t cq, LCI_request_t* request)
{
  LCII_cq_t* cq_ptr = cq;
#ifdef LCI_USE_MUTEX_CQ
  LCIU_acquire_spinlock(&cq_ptr->spinlock);
  LCII_context_t* ctx = LCM_dq_pop_bot(&cq_ptr->dequeue);
  LCIU_release_spinlock(&cq_ptr->spinlock);
#else
  LCII_context_t* ctx = LCM_aqueue_pop(cq_ptr);
#endif
  if (ctx == NULL) return LCI_ERR_RETRY;
  *request = LCII_ctx2req(ctx);
  return LCI_OK;
}

LCI_error_t LCI_queue_wait(LCI_comp_t cq, LCI_request_t* request)
{
  LCII_cq_t* cq_ptr = cq;
  LCII_context_t* ctx = NULL;
  while (ctx == NULL) {
#ifdef LCI_USE_MUTEX_CQ
    LCIU_acquire_spinlock(&cq_ptr->spinlock);
    ctx = LCM_dq_pop_bot(&cq_ptr->dequeue);
    LCIU_release_spinlock(&cq_ptr->spinlock);
#else
    ctx = LCM_aqueue_pop(cq_ptr);
#endif
  }
  *request = LCII_ctx2req(ctx);
  return LCI_OK;
}

LCI_error_t LCI_queue_pop_multiple(LCI_comp_t cq, size_t request_count,
                                   LCI_request_t* requests,
                                   size_t* return_count)
{
  LCII_cq_t* cq_ptr = cq;
  int count = 0;
  LCII_context_t* ctx;
  while (count < request_count) {
#ifdef LCI_USE_MUTEX_CQ
    LCIU_acquire_spinlock(&cq_ptr->spinlock);
    ctx = LCM_dq_pop_bot(&cq_ptr->dequeue);
    LCIU_release_spinlock(&cq_ptr->spinlock);
#else
    ctx = LCM_aqueue_pop(cq_ptr);
#endif
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

LCI_error_t LCI_queue_wait_multiple(LCI_comp_t cq, size_t request_count,
                                    LCI_request_t* requests)
{
  LCII_cq_t* cq_ptr = cq;
  int count = 0;
  LCII_context_t* ctx;
  while (count < request_count) {
#ifdef LCI_USE_MUTEX_CQ
    LCIU_acquire_spinlock(&cq_ptr->spinlock);
    ctx = LCM_dq_pop_bot(&cq_ptr->dequeue);
    LCIU_release_spinlock(&cq_ptr->spinlock);
#else
    ctx = LCM_aqueue_pop(cq_ptr);
#endif
    if (ctx != NULL) {
      requests[count] = LCII_ctx2req(ctx);
      ++count;
    } else {
      continue;
    }
  }
  return LCI_OK;
}

LCI_error_t LCI_queue_len(LCI_comp_t cq, size_t* len)
{
#ifdef LCI_USE_MUTEX_CQ
  LCII_cq_t* cq_ptr = cq;
  *len = LCM_dq_size(cq_ptr->dequeue);
#else
  *len = 0;
#endif
  return LCI_ERR_FEATURE_NA;
}