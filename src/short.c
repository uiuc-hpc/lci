#include "lci.h"
#include "lci_priv.h"
#include "lc/pool.h"

LCI_Status LCI_Sends(void* src, size_t size, int rank, int tag, LCI_Endpoint ep)
{
  struct lc_rep* rep = &(ep->rep[rank]);
  lc_server_sends(ep->server, rep->handle, src, size,
                  MAKE_PROTO(ep->gid, LC_PROTO_DATA, tag));
  return LCI_OK;
}

LCI_Status LCI_Puts(void* src, size_t size, int rank, int rma_id, int offset, LCI_Endpoint ep)
{
  struct lc_rep* rep = &(ep->rep[rank]);
  assert(rma_id == 0 && "fixme");
  lc_server_puts(ep->server, rep->handle, src, rep->base, offset,
                 rep->rkey, size);
  return LCI_OK;
}

LCI_Status LCI_Recvs(void* src, size_t size, int rank, int tag, LCI_Endpoint ep, LCI_Sync sync, LCI_Request* req)
{
  lc_init_req(src, size, req);
  req->sync = sync;
  lc_key key = lc_make_key(rank, tag);
  lc_value value = (lc_value)req;
  if (!lc_hash_insert(ep->tbl, key, &value, CLIENT)) {
    lc_packet* p = (lc_packet*) value;
    memcpy(src, p->data.buffer, p->context.req->size);
    req->size = p->context.req->size;
    LCI_Sync_signal(sync);
    lc_pk_free_data(ep, p);
  }
  return LCI_OK;
}

/*
LCI_Status LCI_Putss(void* src, size_t size, int rank, uintptr_t addr,
                   int meta, LCI_Endpoint ep)
{
  struct lc_rep* rep = &(ep->rep[rank]);
  lc_server_putss(ep->server, rep->handle, src, rep->base, (uint32_t) (addr - rep->base),
                  rep->rkey, MAKE_PROTO(ep->gid, LC_PROTO_LONG, meta), size);
  return LCI_OK;
}
*/
