#include "lc.h"

#include "lc_priv.h"
#include "lc/pool.h"

lc_status lc_sends(void* src, size_t size, int rank, lc_meta tag, lc_ep ep)
{
  struct lci_rep* rep = &(ep->rep[rank]);
  lc_server_sends(ep->handle, rep->handle, src, size,
                  MAKE_PROTO(rep->gid, LC_PROTO_DATA, tag));
  return LC_OK;
}

lc_status lc_putsd(void* src, size_t size, int rank, lc_meta tag, lc_ep ep)
{
  return lc_sends(src, size, rank, tag, ep);
}

lc_status lc_puts(void* src, size_t size, int rank, uintptr_t addr, lc_ep ep)
{
  struct lci_rep* rep = &(ep->rep[rank]);
  lc_server_puts(ep->handle, rep->handle, src, rep->base,
                 (uint32_t) (addr - rep->base), rep->rkey, size);
  return LC_OK;
}

lc_status lc_putss(void* src, size_t size, int rank, uintptr_t addr,
                   lc_meta meta, lc_ep ep)
{
  struct lci_rep* rep = &(ep->rep[rank]);
  lc_server_putss(ep->handle, rep->handle, src, rep->base, (uint32_t) (addr - rep->base),
                  rep->rkey, MAKE_PROTO(rep->gid, LC_PROTO_LONG, meta), size);
  return LC_OK;
}
