#include "lc.h"

#include "lc_priv.h"
#include "lc/pool.h"

lc_status lc_sendl(void* src, size_t size, int rank, lc_meta tag, lc_ep ep,
                   lc_sync* ce)
{
  LC_POOL_GET_OR_RETN(ep->pkpool, p);
  lc_sync_reset(ce);
  lci_pk_init(ep, -1, LC_PROTO_RTS, p);
  struct lci_rep* rep = &(ep->rep[rank]);
  p->context.req = &(p->context.req_s);
  lci_init_req(src, size, p->context.req);
  lci_prepare_rts(src, size, ep->gid, ce, p);
  lc_server_sendm(ep->handle, rep->handle,
                  sizeof(struct packet_rts), p,
                  MAKE_PROTO(rep->gid, LC_PROTO_RTS, tag));
  return LC_OK;
}

lc_status lc_putld(void* src, size_t size, int rank, lc_meta tag, lc_ep ep, lc_sync* ce)
{
  lc_sendl(src, size, rank, tag, ep, ce);
  return LC_OK;
}

lc_status lc_putl(void* src, size_t size, int rank, uintptr_t addr,
                  lc_ep ep, lc_sync* ce)
{
  LC_POOL_GET_OR_RETN(ep->pkpool, p);
  lci_pk_init(ep, -1, LC_PROTO_LONG, p);
  p->data.rts.ce = (uintptr_t) ce;
  struct lci_rep* rep = &(ep->rep[rank]);
  lc_server_putl(ep->handle, rep->handle, src, rep->base, (uint32_t) (addr - rep->base),
                 rep->rkey, size, p);
  return LC_OK;
}

lc_status lc_putls(void* src, size_t size, int rank, uintptr_t addr, lc_meta meta,
                   lc_ep ep, lc_sync* ce)
{
  LC_POOL_GET_OR_RETN(ep->pkpool, p);
  lci_pk_init(ep, -1, LC_PROTO_LONG, p);
  p->data.rts.ce = (uintptr_t) ce;
  struct lci_rep* rep = &(ep->rep[rank]);
  lc_server_putls(ep->handle, rep->handle, src, rep->base, (uint32_t) (addr - rep->base),
                  rep->rkey, size,
                  MAKE_PROTO(rep->gid, LC_PROTO_LONG, meta), p);
  return LC_OK;
}

lc_status lc_recvl(void* src, size_t size, int rank, lc_meta tag, lc_ep ep,
                   lc_req* req)
{
  lci_init_req(src, size, req);
  struct lci_rep* rep = &ep->rep[rank];
  req->rhandle = rep->handle;
  lc_key key = lc_make_key(rank, tag);
  lc_value value = (lc_value)req;
  if (!lc_hash_insert(ep->tbl, key, &value, CLIENT)) {
    lc_packet* p = (lc_packet*) value;
    req->size = p->data.rts.size;
    p->context.req = req;
    lci_handle_rts(ep, p);
  }
  return LC_OK;
}
