#include "lci.h"

LCI_error_t LCI_sync_create(void* sync) {
  *(uint64_t*) sync = 0;
  return LCI_OK;
}

LCI_error_t LCI_one2one_set_full(void* sync) {
  *(uint64_t*)sync = 1;
  return LCI_OK;
}

LCI_error_t LCI_one2one_wait_empty(void* sync) {
  while (*(uint64_t*)sync)
    ;
  return LCI_OK;
}

LCI_error_t LCI_one2one_wait_full(void* sync) {
  while (!*(uint64_t*)sync)
    ;
  return LCI_OK;
}

int LCI_one2one_test_empty(void* sync) {
  return (*(uint64_t*)sync == 0);
}

LCI_error_t LCI_one2one_set_empty(void* sync) {
  *(uint64_t*)sync = 0;
  return LCI_OK;
}
