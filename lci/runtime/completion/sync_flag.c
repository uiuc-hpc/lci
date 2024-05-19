#include "runtime/lcii.h"

struct LCII_sync_t {
  atomic_uint_fast64_t top;
  atomic_uint_fast64_t top2;
  LCIU_CACHE_PADDING(2 * sizeof(atomic_uint_fast64_t));
  atomic_uint_fast64_t tail;
  LCIU_CACHE_PADDING(sizeof(atomic_uint_fast64_t));
  int threshold;
  LCII_context_t** ctx;
};

LCI_error_t LCI_sync_create(LCI_device_t device, int threshold,
                            LCI_comp_t* completion)
{
  // we don't need device for this simple synchronizer
  (void)device;
  LCI_DBG_Assert(threshold > 0, "threshold (%d) <= 0!\n", threshold);
  LCII_sync_t* sync = LCIU_malloc(sizeof(LCII_sync_t));
  sync->threshold = threshold;
  atomic_init(&sync->top, 0);
  atomic_init(&sync->top2, 0);
  atomic_init(&sync->tail, 0);
  sync->ctx = LCIU_malloc(sizeof(LCII_context_t*) * sync->threshold);
  *completion = sync;
  atomic_thread_fence(LCIU_memory_order_seq_cst);
  return LCI_OK;
}

LCI_error_t LCI_sync_free(LCI_comp_t* completion)
{
  atomic_thread_fence(LCIU_memory_order_seq_cst);
  LCII_sync_t* sync = *completion;
  LCI_DBG_Assert(sync != NULL, "synchronizer is a NULL pointer!\n");
  LCIU_free(sync->ctx);
  LCIU_free(sync);
  *completion = NULL;
  return LCI_OK;
}

// Assumption: no LCI_sync_wait/test will succeed when
// LCI_sync_signal is called. Otherwise, this sync can receive more signals than
// its threshold.
LCI_error_t LCII_sync_signal(LCI_comp_t completion, LCII_context_t* ctx)
{
  LCII_sync_t* sync = completion;
  LCI_DBG_Assert(sync != NULL, "synchronizer is a NULL pointer!\n");
#ifdef LCI_USE_PERFORMANCE_COUNTER
  ctx->time = LCT_now();
#endif
  uint_fast64_t tail = 0;
  uint_fast64_t pos = 0;
  if (sync->threshold > 1) {
    pos = atomic_fetch_add_explicit(&sync->top, 1, LCIU_memory_order_relaxed);
    tail = atomic_load_explicit(&sync->tail, LCIU_memory_order_acquire);
  }
  LCI_DBG_Assert(pos < tail + sync->threshold,
                 "Receive more signals than expected\n");
  sync->ctx[pos - tail] = ctx;
  atomic_fetch_add_explicit(&sync->top2, 1, LCIU_memory_order_release);
  LCII_PCOUNTER_ADD(comp_produce, 1);
  return LCI_OK;
}

LCI_error_t LCI_sync_signal(LCI_comp_t completion, LCI_request_t request)
{
  LCII_context_t* ctx = LCIU_malloc(sizeof(LCII_context_t));
  ctx->rank = request.rank;
  ctx->tag = request.tag;
  ctx->data_type = request.type;
  memcpy(&ctx->data, &request.data, sizeof(ctx->data));
  ctx->user_context = request.user_context;
  LCII_sync_signal(completion, ctx);
  return LCI_OK;
}

// LCI_sync_wait is thread-safe against LCI(I)_sync_signal and other
// LCI_sync_wait/test
LCI_error_t LCI_sync_wait(LCI_comp_t completion, LCI_request_t request[])
{
  LCI_error_t ret;
  do {
    ret = LCI_sync_test(completion, request);
  } while (ret == LCI_ERR_RETRY);
  return ret;
}

// LCI_sync_test is thread-safe against LCI(I)_sync_signal and other
// LCI_sync_wait/test
LCI_error_t LCI_sync_test(LCI_comp_t completion, LCI_request_t request[])
{
  LCII_sync_t* sync = completion;
  LCI_DBG_Assert(sync != NULL, "synchronizer is a NULL pointer!\n");
  int top2 = atomic_load_explicit(&sync->top2, LCIU_memory_order_acquire);
  int tail = atomic_load_explicit(&sync->tail, LCIU_memory_order_acquire);
  if (top2 != tail + sync->threshold) {
    return LCI_ERR_RETRY;
  } else {
    uint_fast64_t expected = tail;
    bool succeed = atomic_compare_exchange_weak_explicit(
        &sync->tail, &expected, top2, LCIU_memory_order_release,
        LCIU_memory_order_relaxed);
    if (succeed) {
      for (int i = 0; i < sync->threshold; ++i) {
        LCII_PCOUNTER_ADD(sync_stay_timer, LCT_now() - sync->ctx[i]->time);
        if (request)
          request[i] = LCII_ctx2req(sync->ctx[i]);
        else
          LCIU_free(sync->ctx[i]);
      }
      LCII_PCOUNTER_ADD(comp_consume, sync->threshold);
      return LCI_OK;
    } else {
      return LCI_ERR_RETRY;
    }
  }
}
