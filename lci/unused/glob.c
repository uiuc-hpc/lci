#include "lc.h"
#include "lc_priv.h"

int lc_glob_mark(lc_ep ep)
{
  LCII_MEM_FENCE();
  return ep->completed;
}
