#include "lc.h"

#include "lc_priv.h"
#include "lc/pool.h"

lc_status lc_sendl(struct lci_ep *ep, int rank, void* src, size_t size, lc_meta tag, lc_req* req)
{
  LC_POOL_GET_OR_RETN(ep->pkpool, p);
  p->context.ep = ep;
  p->context.poolid = -1;
  struct lci_rep* rep = &(ep->rep[rank]);
  INIT_CTX(req);
  p->data.rts.req = (uintptr_t) req;
  p->data.rts.src_addr = (uintptr_t) src;
  p->data.rts.size = size;
  p->data.rts.rgid = ep->gid;
  lc_server_send(ep->handle, ep, rep->handle, &p->data,
      sizeof(struct packet_rts), p,
      MAKE_PROTO(rep->gid, LC_PROTO_RTS, tag.val));
  return LC_OK;
}

lc_status lc_putld(struct lci_ep *ep, int rank, void* src, size_t size,
                   lc_meta tag, lc_req* req)
{
  lc_sendl(ep, rank, src, size, tag, req);
  return LC_OK;
}

lc_status lc_recvl(struct lci_ep *ep, int rank, void* src, size_t size, lc_meta tag, lc_req* req)
{
  INIT_CTX(req);
  struct lci_rep* rep = &ep->rep[rank];
  req->rhandle = rep->handle;
  lc_key key = lc_make_key(rank, tag.val);
  lc_value value = (lc_value)req;
  if (!lc_hash_insert(ep->tbl, key, &value, CLIENT)) {
    lc_packet* p = (lc_packet*) value;
    p->context.ep = ep;
    p->context.req = req;
    p->context.proto = LC_PROTO_RTR;
    req->size = p->data.rts.size;
    lci_rdz_prepare(ep, src, size, p);
    lc_server_send(ep->handle, ep, rep->handle, &p->data,
        sizeof(struct packet_rtr), p, LC_PROTO_RTR);
  }
  return LC_OK;
}

