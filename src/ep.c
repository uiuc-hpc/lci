#include "lc.h"
#include "lc_priv.h"

extern struct lc_server* lcg_dev;

lc_status lc_ep_dup(int dev_id, lc_ep_desc desc, lc_ep iep __UNUSED__, lc_ep* oep)
{
  // FIXME: NOT THREAD-SAFE.
  lci_ep_open(&lcg_dev[dev_id], desc.addr | desc.ce , oep);
  return LC_OK;
}

lc_status lc_ep_get_baseaddr(lc_ep ep, size_t size, uintptr_t* addr)
{
  // FIXME: NOT THREAD-SAFE.
  *addr = ep->server->curr_addr;
  ep->server->curr_addr += size;
  return LC_OK;
}

lc_status lc_ep_set_alloc(lc_ep ep, lc_alloc_fn alloc, lc_free_fn free, void* ctx)
{
  ep->alloc = alloc;
  ep->free = free;
  ep->ctx = ctx;
  return LC_OK;
}

lc_status lc_ep_set_handler(lc_ep ep, lc_handler_fn handler, void* ctx)
{
  ep->handler = handler;
  ep->ctx = ctx;
  return LC_OK;
}
