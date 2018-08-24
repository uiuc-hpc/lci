#include "lc.h"

#include "lc_priv.h"
#include "lc/pool.h"

lc_status lc_sendm(struct lci_ep *ep, int rank, void* src, size_t size, lc_meta tag, lc_req* req)
{
  LC_POOL_GET_OR_RETN(ep->dev->pkpool, p);
  p->context.ep = ep;
  p->context.poolid = (size > 128) ? lc_pool_get_local(ep->dev->pkpool) : -1;
  struct lci_rep* rep = &(ep->rep[rank]);
  lc_server_send(ep->dev->handle, ep, rep->handle, src, size, p,
      MAKE_PROTO(rep->gid, LC_PROTO_DATA, tag.val));
  req->flag = 1;
  return LC_OK;
}

lc_status lc_putmd(struct lci_ep *ep, int rank, void* src, size_t size,
                   lc_meta tag, lc_req* req)
{
  lc_sendm(ep, rank, src, size, tag, req);
  return LC_OK;
}

lc_status lc_recvm(struct lci_ep *ep, int rank, void* src, size_t size, lc_meta tag, lc_req* req)
{
  INIT_CTX(req);
  struct lci_rep* rep = &ep->rep[rank];
  req->rhandle = rep->handle;
  lc_key key = lc_make_key(rank, tag.val);
  lc_value value = (lc_value)req;
  if (!lc_hash_insert(ep->tbl, key, &value, CLIENT)) {
    lc_packet* p = (lc_packet*) value;
    memcpy(src, p->data.buffer, p->context.req->size);
    req->size = p->context.req->size;
    req->flag = 1;
    lc_pool_put(ep->dev->pkpool, p);
  }
  return LC_OK;
}
