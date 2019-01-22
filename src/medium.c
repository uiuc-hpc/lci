#include "lci.h"

#include "lci_priv.h"
#include "lc/pool.h"

LCI_Status LCI_Sendm(void* src, size_t size, int rank, int tag, LCI_Endpoint ep)
{
  LC_POOL_GET_OR_RETN(ep->pkpool, p);
  lc_pk_init(ep, (size > 1024) ? lc_pool_get_local(ep->pkpool) : -1,
              LC_PROTO_DATA, p);
  struct lc_rep* rep = &(ep->rep[rank]);
  memcpy(p->data.buffer, src, size);
  lc_server_sendm(ep->server, rep->handle, size, p,
                  MAKE_PROTO(ep->gid, LC_PROTO_DATA, tag));
  return LCI_OK;
}

LCI_Status LCI_Putm(void* src, size_t size, int rank, int rma_id, int offset, LCI_Endpoint ep)
{
  LC_POOL_GET_OR_RETN(ep->pkpool, p);
  assert(rma_id == 0);
  lc_pk_init(ep, (size > 1024) ? lc_pool_get_local(ep->pkpool) : -1,
              LC_PROTO_DATA, p);
  struct lc_rep* rep = &(ep->rep[rank]);
  memcpy(&p->data, src, size);
  lc_server_putm(ep->server, rep->handle, rep->base, offset,
                 rep->rkey, size, p);
  return LCI_OK;
}

/*
LCI_Status LCI_Putms(void* src, size_t size, int rank, uintptr_t addr, int meta, LCI_Endpoint ep)
{
  LC_POOL_GET_OR_RETN(ep->pkpool, p);
  lc_pk_init(ep, (size > 1024) ? lc_pool_get_local(ep->pkpool) : -1,
              LC_PROTO_DATA, p);
  struct lc_rep* rep = &(ep->rep[rank]);
  memcpy(&p->data, src, size);
  lc_server_putms(ep->server, rep->handle, rep->base, (uint32_t) (addr - rep->base),
                  rep->rkey, size, MAKE_PROTO(ep->gid, LC_PROTO_LONG, meta), p);
  return LCI_OK;
}
*/

LCI_Status LCI_Recvm(void* src, size_t size, int rank, int tag, LCI_Endpoint ep, LCI_Sync sync, LCI_Request* req)
{
  lc_init_req(src, size, req);
  req->sync = sync;
  lc_key key = lc_make_key(rank, tag);
  lc_value value = (lc_value)req;
  if (!lc_hash_insert(ep->tbl, key, &value, CLIENT)) {
    lc_packet* p = (lc_packet*) value;
    memcpy(src, p->data.buffer, p->context.req->size);
    req->size = p->context.req->size;
    LCI_Sync_signal(req->sync);
    lc_pk_free_data(ep, p);
  }
  return LCI_OK;
}
