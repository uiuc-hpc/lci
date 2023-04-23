#include "runtime/lcii.h"

struct LCII_sync_t {
  atomic_int count;
  atomic_int confirm;
  int threshold;
  LCII_context_t** ctx;
};

LCI_error_t LCI_sync_create(LCI_device_t device, int threshold,
                            LCI_comp_t* completion)
{
  // we don't need device for this simple synchronizer
  (void)device;
  LCM_DBG_Assert(threshold > 0, "threshold (%d) <= 0!\n", threshold);
  LCII_sync_t* sync = LCIU_malloc(sizeof(LCII_sync_t));
  sync->threshold = threshold;
  atomic_init(&sync->count, 0);
  atomic_init(&sync->confirm, 0);
  sync->ctx = LCIU_malloc(sizeof(LCII_context_t*) * sync->threshold);
  *completion = sync;
  atomic_thread_fence(LCIU_memory_order_seq_cst);
  return LCI_OK;
}

LCI_error_t LCI_sync_free(LCI_comp_t* completion)
{
  atomic_thread_fence(LCIU_memory_order_seq_cst);
  LCII_sync_t* sync = *completion;
  LCM_DBG_Assert(sync != NULL, "synchronizer is a NULL pointer!\n");
  LCIU_free(sync->ctx);
  LCIU_free(sync);
  *completion = NULL;
  return LCI_OK;
}

LCI_error_t LCII_sync_signal(LCI_comp_t completion, LCII_context_t* ctx)
{
  LCII_sync_t* sync = completion;
  LCM_DBG_Assert(sync != NULL, "synchronizer is a NULL pointer!\n");
  int pos = 0;
  if (sync->threshold > 1)
    pos = atomic_fetch_add_explicit(&sync->count, 1, LCIU_memory_order_relaxed);
  LCM_DBG_Assert(pos < sync->threshold, "Receive more signals than expected\n");
  sync->ctx[pos] = ctx;
  if (sync->threshold > 1)
    atomic_fetch_add_explicit(&sync->confirm, 1, LCIU_memory_order_release);
  else
    atomic_store_explicit(&sync->confirm, 1, LCIU_memory_order_release);
  return LCI_OK;
}

LCI_error_t LCI_sync_signal(LCI_comp_t completion, LCI_request_t request)
{
  LCII_context_t* ctx = LCIU_malloc(sizeof(LCII_context_t));
  ctx->rank = request.rank;
  ctx->tag = request.tag;
  ctx->data_type = request.type;
  ctx->data = request.data;
  ctx->user_context = request.user_context;

  LCII_sync_signal(completion, ctx);
  return LCI_OK;
}

// LCI_sync_wait is thread-safe against LCI(I)_sync_signal
// but not thread-safe against other LCI_sync_wait/test
LCI_error_t LCI_sync_wait(LCI_comp_t completion, LCI_request_t request[])
{
  LCII_sync_t* sync = completion;
  LCM_DBG_Assert(sync != NULL, "synchronizer is a NULL pointer!\n");
  while (atomic_load_explicit(&sync->confirm, LCIU_memory_order_acquire) <
         sync->threshold)
    continue;
  if (request)
    for (int i = 0; i < sync->threshold; ++i) {
      request[i] = LCII_ctx2req(sync->ctx[i]);
    }
  else
    for (int i = 0; i < sync->threshold; ++i) {
      LCIU_free(sync->ctx[i]);
    }
  atomic_store_explicit(&sync->confirm, 0, LCIU_memory_order_relaxed);
  if (sync->threshold > 1)
    atomic_store_explicit(&sync->count, 0, LCIU_memory_order_relaxed);
  return LCI_OK;
}

// LCI_sync_test is thread-safe against LCI(I)_sync_signal
// but not thread-safe against other LCI_sync_wait/test
LCI_error_t LCI_sync_test(LCI_comp_t completion, LCI_request_t request[])
{
  LCII_sync_t* sync = completion;
  LCM_DBG_Assert(sync != NULL, "synchronizer is a NULL pointer!\n");
  if (atomic_load_explicit(&sync->confirm, LCIU_memory_order_acquire) <
      sync->threshold) {
    return LCI_ERR_RETRY;
  } else {
    LCM_DBG_Assert(sync->confirm == sync->threshold,
                   "Receive more signals (%d) than expected (%d)\n",
                   sync->confirm, sync->threshold);
    if (request)
      for (int i = 0; i < sync->threshold; ++i) {
        request[i] = LCII_ctx2req(sync->ctx[i]);
      }
    else
      for (int i = 0; i < sync->threshold; ++i) {
        LCIU_free(sync->ctx[i]);
      }
    atomic_store_explicit(&sync->confirm, 0, LCIU_memory_order_relaxed);
    if (sync->threshold > 1)
      atomic_store_explicit(&sync->count, 0, LCIU_memory_order_relaxed);
    return LCI_OK;
  }
}
