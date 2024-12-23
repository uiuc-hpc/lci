#include "runtime/lcii.h"

LCT_queue_type_t cq_type;

void LCII_env_init_cq_type()
{
  const LCT_queue_type_t cq_type_default = LCT_QUEUE_ARRAY_ATOMIC_FAA;
  LCT_dict_str_int_t dict[] = {
      {NULL, cq_type_default},
      {"array_atomic_faa", LCT_QUEUE_ARRAY_ATOMIC_FAA},
      {"array_atomic_cas", LCT_QUEUE_ARRAY_ATOMIC_CAS},
      {"array_atomic_basic", LCT_QUEUE_ARRAY_ATOMIC_BASIC},
      {"array_mutex", LCT_QUEUE_ARRAY_MUTEX},
      {"std_mutex", LCT_QUEUE_STD_MUTEX},
      {"lprq", LCT_QUEUE_LPRQ},
  };
  bool succeed = LCT_str_int_search(dict, sizeof(dict) / sizeof(dict[0]),
                                    getenv("LCI_CQ_TYPE"), cq_type_default,
                                    (int*)&cq_type);
  if (!succeed) {
    LCI_Warn("Unknown LCI_CQ_TYPE %s. Use the default type: array_atomic_faa\n",
             getenv("LCI_CQ_TYPE"));
  }
  LCI_Log(LCI_LOG_INFO, "comp", "Set LCI_CQ_TYPE to %d\n", cq_type);
}

LCI_error_t LCI_queue_create(LCI_device_t device, LCI_comp_t* cq)
{
#ifdef LCI_USE_INLINE_CQ
  LCII_cq_t* cq_ptr = LCIU_malloc(sizeof(LCII_cq_t));
  LCM_aqueue_init(cq_ptr, LCI_DEFAULT_QUEUE_LENGTH);
  *cq = cq_ptr;
#else
  *cq = LCT_queue_alloc(cq_type, LCI_DEFAULT_QUEUE_LENGTH);
#endif
  return LCI_OK;
}

LCI_error_t LCI_queue_createx(LCI_device_t device, size_t max_length,
                              LCI_comp_t* cq)
{
#ifdef LCI_USE_INLINE_CQ
  LCII_cq_t* cq_ptr = LCIU_malloc(sizeof(LCII_cq_t));
  LCM_aqueue_init(cq_ptr, max_length);
  *cq = cq_ptr;
#else
  *cq = LCT_queue_alloc(cq_type, max_length);
#endif
  return LCI_OK;
}

LCI_error_t LCI_queue_free(LCI_comp_t* cq_ptr)
{
#ifdef LCI_USE_INLINE_CQ
  LCM_aqueue_fina(*cq_ptr);
  LCIU_free(*cq_ptr);
  *cq_ptr = NULL;
#else
  LCT_queue_free((LCT_queue_t*)cq_ptr);
#endif
  return LCI_OK;
}

LCI_error_t LCI_queue_pop(LCI_comp_t cq, LCI_request_t* request)
{
#ifdef LCI_USE_INLINE_CQ
  LCII_context_t* ctx = LCM_aqueue_pop(cq);
#else
  LCII_context_t* ctx = LCT_queue_pop(cq);
#endif
  if (ctx == NULL) return LCI_ERR_RETRY;
  LCII_PCOUNTER_ADD(cq_stay_timer, LCT_now() - ctx->time);
  *request = LCII_ctx2req(ctx);
  LCII_PCOUNTER_ADD(comp_consume, 1);
  return LCI_OK;
}

LCI_error_t LCI_queue_wait(LCI_comp_t cq, LCI_request_t* request)
{
  LCII_context_t* ctx = NULL;
  while (ctx == NULL) {
#ifdef LCI_USE_INLINE_CQ
    ctx = LCM_aqueue_pop((LCII_cq_t*)cq);
#else
    ctx = LCT_queue_pop(cq);
#endif
  }
  LCII_PCOUNTER_ADD(cq_stay_timer, LCT_now() - ctx->time);
  *request = LCII_ctx2req(ctx);
  LCII_PCOUNTER_ADD(comp_consume, 1);
  return LCI_OK;
}

LCI_error_t LCI_queue_pop_multiple(LCI_comp_t cq, size_t request_count,
                                   LCI_request_t* requests,
                                   size_t* return_count)
{
  int count = 0;
  LCII_context_t* ctx;
  while (count < request_count) {
#ifdef LCI_USE_INLINE_CQ
    ctx = LCM_aqueue_pop(cq);
#else
    ctx = LCT_queue_pop(cq);
#endif
    if (ctx != NULL) {
      LCII_PCOUNTER_ADD(cq_stay_timer, LCT_now() - ctx->time);
      requests[count] = LCII_ctx2req(ctx);
      ++count;
    } else {
      break;
    }
  }
  *return_count = count;
  LCII_PCOUNTER_ADD(comp_consume, (int64_t)request_count);
  return LCI_OK;
}

LCI_error_t LCI_queue_wait_multiple(LCI_comp_t cq, size_t request_count,
                                    LCI_request_t* requests)
{
  int count = 0;
  LCII_context_t* ctx;
  while (count < request_count) {
#ifdef LCI_USE_INLINE_CQ
    ctx = LCM_aqueue_pop(cq);
#else
    ctx = LCT_queue_pop(cq);
#endif
    if (ctx != NULL) {
      LCII_PCOUNTER_ADD(cq_stay_timer, LCT_now() - ctx->time);
      requests[count] = LCII_ctx2req(ctx);
      ++count;
    } else {
      continue;
    }
  }
  LCII_PCOUNTER_ADD(comp_consume, (int64_t)request_count);
  return LCI_OK;
}

LCI_error_t LCI_queue_len(LCI_comp_t cq, size_t* len)
{
  return LCI_ERR_FEATURE_NA;
}