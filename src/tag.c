#include "lc.h"
#include "lc_priv.h"

lc_status lc_send(void* src, size_t size, int rank, lc_meta tag, lc_ep ep, lc_sync* ce)
{
  int ret;
  if (size <= LC_MAX_INLINE) {
    ret = lc_sends(src, size, rank, tag, ep);
    if (ret == LC_OK)
      lc_sync_signal(ce);
    return ret;
  } else if (size <=LC_PACKET_SIZE) {
    ret = lc_sendm(src, size, rank, tag, ep);
    if (ret == LC_OK)
      lc_sync_signal(ce);
    return ret;
  } else {
    return lc_sendl(src, size, rank, tag, ep, ce);
  }
}

lc_status lc_recv(void* src, size_t size, int rank, lc_meta tag, lc_ep ep, lc_req* req)
{
  lci_init_req(src, size, req);
  struct lci_rep* rep = &ep->rep[rank];
  req->rhandle = rep->handle;
  lc_key key = lc_make_key(rank, tag);
  lc_value value = (lc_value)req;
  if (!lc_hash_insert(ep->tbl, key, &value, CLIENT)) {
    lc_packet* p = (lc_packet*) value;
    if (p->context.proto & LC_PROTO_RTS) {
      req->size = p->data.rts.size;
      p->context.req = req;
      lci_handle_rts(ep, p);
    } else {
      memcpy(src, p->data.buffer, p->context.req->size);
      req->size = p->context.req->size;
      lc_sync_signal(&req->sync);
      lc_pool_put(ep->pkpool, p);
    }
  }
  return LC_OK;
}
