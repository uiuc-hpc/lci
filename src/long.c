#include "lci.h"

#include "lci_priv.h"
#include "lc/pool.h"

LCI_Status LCI_Sendl(void* src, size_t size, int rank, int tag, LCI_Endpoint ep,
                     void* context)
{
  LC_POOL_GET_OR_RETN(ep->pkpool, p);
  lc_pk_init(ep, -1, LC_PROTO_RTS, p);
  struct lc_rep* rep = &(ep->rep[rank]);
  lc_prepare_rts(src, size, context, p);
  lc_server_sendm(ep->server, rep->handle,
                  sizeof(struct packet_rts), p,
                  MAKE_PROTO(ep->gid, LC_PROTO_RTS, tag));
  return LCI_OK;
}

LCI_Status LCI_Putl(void* src, size_t size, int rank, int rma_id, int offset,
                  LCI_Endpoint ep, void* ce)
{
  LC_POOL_GET_OR_RETN(ep->pkpool, p);
  lc_pk_init(ep, -1, LC_PROTO_LONG, p);
  p->data.rts.ce = (uintptr_t) ce;

  struct lc_rep* rep = &(ep->rep[rank]);
  lc_server_putl(ep->server, rep->handle, src, rep->base, offset,
                 rep->rkey, size, p);
  return LCI_OK;
}

/*
LCI_Status LCI_Putls(void* src, size_t size, int rank, uintptr_t addr, int meta,
                   LCI_Endpoint ep, lc_send_cb cb, void* ce)
{
  LC_POOL_GET_OR_RETN(ep->pkpool, p);
  lc_pk_init(ep, -1, LC_PROTO_LONG, p);
  p->data.rts.cb = cb;
  p->data.rts.ce = (uintptr_t) ce;
  struct lc_rep* rep = &(ep->rep[rank]);
  lc_server_putls(ep->server, rep->handle, src, rep->base, (uint32_t) (addr - rep->base),
                  rep->rkey, size,
                  MAKE_PROTO(ep->gid, LC_PROTO_LONG, meta), p);
  return LCI_OK;
}*/

LCI_Status LCI_Recvl(void* src, size_t size, int rank, int tag, LCI_Endpoint ep,
                     LCI_Sync sync, LCI_Request* req)
{
  lc_init_req(src, size, req);
  req->sync = sync;
  struct lc_rep* rep = &ep->rep[rank];
  req->__reserved__ = rep->handle;
  lc_key key = lc_make_key(rank, tag);
  lc_value value = (lc_value)req;
  if (!lc_hash_insert(ep->tbl, key, &value, CLIENT)) {
    lc_packet* p = (lc_packet*) value;
    req->size = p->data.rts.size;
    p->context.req = req;
    lc_handle_rts(ep, p);
  }
  return LCI_OK;
}
