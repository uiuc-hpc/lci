#include "lcii.h"

#define LCII_SYNC_FULL 1
#define LCII_SYNC_EMPTY 0

struct LCII_sync_t {
  volatile uint64_t flag;
  LCII_context_t *ctx;
};

LCI_error_t LCI_sync_create(LCI_device_t device, LCI_sync_type_t sync_type,
                            LCI_comp_t* completion)
{
  // we don't need device for this simple synchronizer
  (void) device;
  LCM_DBG_Assert(sync_type == LCI_SYNC_SIMPLE, "Currently LCI only supports LCI_SYNC_SIMPLE type.\n");
  LCII_sync_t *sync = LCIU_malloc(sizeof(LCII_sync_t));
  sync->flag = LCII_SYNC_EMPTY;
  *completion = sync;
  return LCI_OK;
}

LCI_error_t LCI_sync_free(LCI_comp_t *completion) {
  LCIU_free(*completion);
  *completion = NULL;
  return LCI_OK;
}

LCI_error_t LCI_sync_signal(LCI_comp_t completion, LCII_context_t* ctx)
{
  LCII_sync_t *sync = completion;
  sync->ctx = ctx;
  sync->flag = LCII_SYNC_FULL;
  return LCI_OK;
}

LCI_error_t LCI_sync_wait(LCI_comp_t completion, LCI_request_t* request)
{
  LCII_sync_t *sync = completion;
  while (sync->flag == LCII_SYNC_EMPTY) continue;
  if (request)
    *request = LCII_ctx2req(sync->ctx);
  else
    LCIU_free(sync->ctx);
  sync->flag = LCII_SYNC_EMPTY;
  return LCI_OK;
}

LCI_error_t LCI_sync_test(LCI_comp_t completion, LCI_request_t* request)
{
  LCII_sync_t *sync = completion;
  if (sync->flag == LCII_SYNC_EMPTY) {
    return LCI_ERR_RETRY;
  } else {
    if (request) {
      *request = LCII_ctx2req(sync->ctx);
    } else {
      LCIU_free(sync->ctx);
    }
    sync->flag = LCII_SYNC_EMPTY;
    return LCI_OK;
  }
}
