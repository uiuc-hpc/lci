#include "lci.h"
#include "hashtable.h"
#include "lock.h"
#include "macro.h"

LCI_error_t LCI_MT_create(uint32_t length, LCI_MT_t *mt) {
  struct LCI_MT_s** mt_s = (struct LCI_MT_s**) mt;
  *mt_s = (struct LCI_MT_s*) create_table(1 << TBL_BIT_SIZE);
  return LCI_OK;
}

LCI_error_t LCI_MT_free(LCI_MT_t *mt) {
  free(*mt);
  return LCI_OK;
}
