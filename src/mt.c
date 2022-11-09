#include "lcii.h"

LCI_error_t LCII_mt_init(LCI_mt_t* mt, uint32_t length)
{
  struct LCI_MT_s** mt_s = (struct LCI_MT_s**)mt;
  *mt_s = (struct LCI_MT_s*)create_table(1 << TBL_BIT_SIZE);
  return LCI_OK;
}

LCI_error_t LCII_mt_free(LCI_mt_t* mt)
{
  free(*mt);
  return LCI_OK;
}
