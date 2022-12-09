#include "runtime/lcii.h"

LCI_error_t LCII_mt_init(LCI_mt_t* mt, uint32_t length)
{
  LCM_hashtable_t** mt_s = (LCM_hashtable_t**)mt;
  *mt_s = (LCM_hashtable_t*)LCMI_hashtable_create_table(
      1 << LCM_HASHTABLE_BIT_SIZE);
  return LCI_OK;
}

LCI_error_t LCII_mt_free(LCI_mt_t* mt)
{
  free(*mt);
  return LCI_OK;
}
