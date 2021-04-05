#include "lci.h"

#include <abt.h>

typedef struct lci_abt_sync {
  ABT_cond cond;
  ABT_mutex mutex;
  volatile int flag;
} lci_abt_sync;

LCI_error_t LCI_sync_create(LCI_device_t device, LCI_sync_type_t sync_type,
                            LCI_comp_t* sync)
{
  lci_abt_sync** sync_abt = (lci_abt_sync**) sync;
  lci_abt_sync* new_sync = (lci_abt_sync*) calloc(sizeof(struct lci_abt_sync), 1);
  ABT_cond_create(&new_sync->cond);
  ABT_mutex_create(&new_sync->mutex);
  *sync_abt = new_sync;
  return LCI_OK;
}

LCI_error_t LCI_sync_wait(LCI_comp_t sync, LCI_request_t* request)
{
  lci_abt_sync* sync_abt = *(lci_abt_sync**)request;
  ABT_mutex_lock(sync_abt->mutex);
  sync_abt->flag = 1;
  ABT_cond_signal(sync_abt->cond);
  ABT_mutex_unlock(sync_abt->mutex);
  return LCI_OK;
}

LCI_error_t LCI_one2one_set_empty(void* sync) {
  lci_abt_sync* sync_abt = *(lci_abt_sync**) sync;
  // ABT_mutex_lock(sync_abt->mutex);
  sync_abt->flag = 0;
  // ABT_cond_signal(sync_abt->cond);
  // ABT_mutex_unlock(sync_abt->mutex);
  return LCI_OK;
}

LCI_error_t LCI_one2one_wait_empty(void* sync) {
  lci_abt_sync* sync_abt = *(lci_abt_sync**) sync;
  // ABT_mutex_lock(sync_abt->mutex);
  while (sync_abt->flag) {
    // ABT_cond_wait(sync_abt->cond, sync_abt->mutex);
  }
  // ABT_mutex_unlock(sync_abt->mutex);
  return LCI_OK;
}

LCI_error_t LCI_one2one_wait_full(void* sync) {
  lci_abt_sync* sync_abt = *(lci_abt_sync**) sync;
  ABT_mutex_lock(sync_abt->mutex);
  while (!sync_abt->flag) {
    ABT_cond_wait(sync_abt->cond, sync_abt->mutex);
  }
  ABT_mutex_unlock(sync_abt->mutex);
  return LCI_OK;
}

int LCI_one2one_test_empty(void* sync) {
  lci_abt_sync* sync_abt = *(lci_abt_sync**) sync;
  return (sync_abt->flag == 0);
}
