#include "lci.h"
#include "lci_priv.h"
#include "lc/pool.h"

LCI_error_t LCI_sendi(void* src, size_t size, int rank, int tag, LCI_endpoint_t ep)
{
  struct lc_rep* rep = &(ep->rep[rank]);
  lc_server_sends(ep->server, rep->handle, src, size,
                  MAKE_PROTO(ep->gid, LC_PROTO_DATA, tag));
  return LCI_OK;
}

LCI_error_t LCI_sendbc(void* src, size_t size, int rank, int tag, LCI_endpoint_t ep)
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

LCI_error_t LCI_sendd(void* src, size_t size, int rank, int tag, LCI_endpoint_t ep,
                     void* sync)
{
  LC_POOL_GET_OR_RETN(ep->pkpool, p);
  lc_pk_init(ep, -1, LC_PROTO_RTS, p);
  struct lc_rep* rep = &(ep->rep[rank]);
  lc_prepare_rts(src, size, sync, p);
  lc_server_sendm(ep->server, rep->handle,
                  sizeof(struct packet_rts), p,
                  MAKE_PROTO(ep->gid, LC_PROTO_RTS, tag));
  return LCI_OK;
}

LCI_error_t LCI_recvi(void* src, size_t size, int rank, int tag, LCI_endpoint_t ep, void* sync)
{
  lc_init_req(src, size, LCI_SYNCL_PTR_TO_REQ_PTR(sync));
  lc_key key = lc_make_key(rank, tag);
  lc_value value = (lc_value) sync;
  LCI_request_t* request = LCI_SYNCL_PTR_TO_REQ_PTR(sync);
  if (!lc_hash_insert(ep->mt, key, &value, CLIENT)) {
    lc_packet* p = (lc_packet*) value;
    memcpy(src, p->data.buffer, p->context.sync->request.length);
    request->length = p->context.sync->request.length;
    lc_ce_dispatch(ep, p, sync, ep->property);
  }
  return LCI_OK;
}

LCI_error_t LCI_recvbc(void* src, size_t size, int rank, int tag, LCI_endpoint_t ep, void* sync)
{
  LCI_request_t* req = LCI_SYNCL_PTR_TO_REQ_PTR(sync);
  lc_init_req(src, size, req);
  lc_key key = lc_make_key(rank, tag);
  lc_value value = (lc_value) sync;
  if (!lc_hash_insert(ep->mt, key, &value, CLIENT)) {
    lc_packet* p = (lc_packet*) value;
    memcpy(src, p->data.buffer, p->context.sync->request.length);
    req->length = p->context.sync->request.length;
    lc_ce_dispatch(ep, p, sync, ep->property);
  }
  return LCI_OK;
}

LCI_error_t LCI_recvd(void* src, size_t size, int rank, int tag, LCI_endpoint_t ep, void* sync)
{
  LCI_request_t* req = LCI_SYNCL_PTR_TO_REQ_PTR(sync);
  lc_init_req(src, size, req);
  struct lc_rep* rep = &ep->rep[rank];
  req->__reserved__ = rep->handle;
  lc_key key = lc_make_key(rank, tag);
  lc_value value = (lc_value) sync;
  if (!lc_hash_insert(ep->mt, key, &value, CLIENT)) {
    lc_packet* p = (lc_packet*) value;
    req->length = p->data.rts.size;
    p->context.sync = sync;
    lc_handle_rts(ep, p);
  }
  return LCI_OK;
}
