#include "lc.h"
#include "lc_priv.h"

extern lc_server* lcg_dev[];

lc_status lc_ep_dup(lc_opt* opt, lc_ep iep __UNUSED__, lc_ep* oep)
{
  lci_ep_open(lcg_dev[opt->dev], opt->desc.addr | opt->desc.ce, oep);
  if (opt->desc.addr == EP_AR_DYN) {
    (*oep)->alloc = opt->alloc;
  }
  if (opt->desc.ce == EP_CE_AM) {
    (*oep)->handler = opt->handler;
  }
  if (opt->glob) {
    (*oep)->cap |= EP_CE_GLOB;
    (*oep)->completed = 0;
  }
  return LC_OK;
}

lc_status lc_ep_get_baseaddr(lc_ep ep, size_t size, uintptr_t* addr)
{
  // FIXME: NOT THREAD-SAFE.
  *addr = ep->server->curr_addr;
  ep->server->curr_addr += size;
  return LC_OK;
}
