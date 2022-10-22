#include "lc.h"
#include "config.h"

#include "lc_priv.h"
#include "lc/pool.h"

#ifdef LC_PKT_RET_LONG
#define LC_LONG_POOL_ID(ep) (lc_pool_get_local_id(ep->pkpool))
#else
#define LC_LONG_POOL_ID(ep) (-1)
#endif

lc_status lc_sendl(void* src, size_t size, int rank, int tag, lc_ep ep,
                   lc_send_cb cb, void* ce)
{
  LC_POOL_GET_OR_RETN(ep->pkpool, p);
  LC_SERVER_CAN_SEND_OR_RET(ep, 2, p);
  lci_pk_init(ep, LC_LONG_POOL_ID(ep), LC_PROTO_RTS, p);
  struct lci_rep* rep = &(ep->rep[rank]);
  lci_prepare_rts(src, size, cb, ce, p);
  p->data.rts.rma_mem = lc_server_rma_reg(ep->server, src, size);
  lc_status ret = lc_server_send_rts(ep->server, rep->handle,
                                     sizeof(struct packet_rts), p,
                                     MAKE_PROTO(ep->gid, LC_PROTO_RTS, tag));
  if (ret != LC_OK)
      lc_pool_put(ep->pkpool, p);
  return ret;
}

lc_status lc_putl(void* src, size_t size, int rank, uintptr_t addr,
                  lc_ep ep, lc_send_cb cb, void* ce)
{
  LC_POOL_GET_OR_RETN(ep->pkpool, p);
  LC_SERVER_CAN_SEND_OR_RET(ep, 1, p);
  lci_pk_init(ep, LC_LONG_POOL_ID(ep), LC_PROTO_LONG, p);
  p->data.rts.cb = cb;
  p->data.rts.ce = (uintptr_t) ce;
  p->context.rma_mem = lc_server_rma_reg(ep->server, src, size);
  struct lci_rep* rep = &(ep->rep[rank]);
  lc_status ret = lc_server_putl(ep->server, rep->handle, src, rep->base,
                                 (uint32_t)(addr - rep->base), rep->rkey, size, p);
  if (ret != LC_OK)
      lc_pool_put(ep->pkpool, p);
  return ret;
}

lc_status lc_putls(void* src, size_t size, int rank, uintptr_t addr, int meta,
                   lc_ep ep, lc_send_cb cb, void* ce)
{
  LC_POOL_GET_OR_RETN(ep->pkpool, p);
  LC_SERVER_CAN_SEND_OR_RET(ep, 1, p);
  lci_pk_init(ep, LC_LONG_POOL_ID(ep), LC_PROTO_LONG, p);
  p->data.rts.cb = cb;
  p->data.rts.ce = (uintptr_t) ce;
  p->context.rma_mem = lc_server_rma_reg(ep->server, src, size);
  struct lci_rep* rep = &(ep->rep[rank]);
  lc_status ret = lc_server_putls(ep->server, rep->handle, src, rep->base,
                                  (uint32_t)(addr - rep->base), rep->rkey, size,
                                  MAKE_PROTO(ep->gid, LC_PROTO_LONG, meta), p);
  if (ret != LC_OK)
      lc_pool_put(ep->pkpool, p);
  return ret;
}

lc_status lc_recvl(void* src, size_t size, int rank, int tag, lc_ep ep,
                   lc_req* req)
{
  LC_SERVER_CAN_SEND_OR_RET(ep, 1, NULL);
  lci_init_req(src, size, req);
  struct lci_rep* rep = &ep->rep[rank];
  req->rhandle = rep->handle;
  lc_key key = lc_make_key(rank, tag);
  lc_value value = (lc_value)req;
  if (!lc_hash_insert(ep->tbl, key, &value, CLIENT)) {
    lc_packet* p = (lc_packet*) value;
    req->size = p->data.rts.size;
    p->context.req = req;
    if (LC_OK != lci_handle_rts(ep, p))
      abort();
  }
  return LC_OK;
}
