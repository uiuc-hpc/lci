#include "lc.h"
#include "lc_priv.h"

int lc_glob_mark(lc_ep ep)
{
  lc_mem_fence();
  return ep->completed;
}
