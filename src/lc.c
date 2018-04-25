#include "lc.h"

#include "lc_priv.h"
#include "lc/pool.h"

#include <assert.h>

struct lci_struct g_lc_info;
int lc_current_id = 1;
__thread int lc_core_id = 0;
int server_deadlock_alert;

lc_status lc_init()
{
  lci_master_ep_init(&g_lc_info.my_ep);
  return LC_OK;
}

lc_status lc_finalize()
{
  return LC_OK;
}

lc_id lc_rank()
{
  return g_lc_info.my_ep.eid;
}

static inline
lc_status lci_send_alloc(struct lci_ep *ep, lc_id tep, void* src, size_t size, lc_sig* lsig, lc_sig* rsig, lc_req* req)
{
  LC_POOL_GET_OR_RETN(ep->pkpool, p);

  if (size <= (int) SHORT_MSG_SIZE) {
    lc_server_send(ep->hw.handle, tep, src, size, p, LC_PROTO_DATA | LC_PROTO_ALLOC | LC_PROTO_QUEUE);
    req->flag = 1;
  } else {
    INIT_CTX(req);
    p->data.rts.req = (uintptr_t) req;
    p->data.rts.src_addr = (uintptr_t) src;
    p->data.rts.size = size;
    lc_server_send(ep->hw.handle, tep, &p->data, sizeof(struct packet_rts), p, LC_PROTO_RTS | LC_PROTO_ALLOC | LC_PROTO_QUEUE);
  }
  return LC_OK;
}

static inline
lc_status lci_send_tag(struct lci_ep *ep, lc_id tep, void* src, size_t size, uint32_t tag, lc_sig* lsig, lc_sig* rsig, lc_req* req)
{
  LC_POOL_GET_OR_RETN(ep->pkpool, p);
  if (size <= (int) SHORT_MSG_SIZE) {
    lc_server_send(ep->hw.handle, tep, src, size, p,
                   MAKE_PROTO(LC_PROTO_DATA | LC_PROTO_TAG, tag));
    req->flag = 1;
  } else {
    INIT_CTX(req);
    p->data.rts.req = (uintptr_t) req;
    p->data.rts.src_addr = (uintptr_t) src;
    p->data.rts.size = size;
    lc_server_send(ep->hw.handle, tep, &p->data, sizeof(struct packet_rts), p,
                   MAKE_PROTO(LC_PROTO_RTS | LC_PROTO_TAG, tag));
  }
  return LC_OK;
}

static inline
lc_status lci_recv_tag(struct lci_ep *ep, lc_id tep, void* src, size_t size, uint32_t tag, lc_sig* lsig, lc_sig* rsig, lc_req* req)
{
  INIT_CTX(req);
  req->tag = tag;
  lc_key key = lc_make_key(tep, tag);
  lc_value value = (lc_value)req;
  if (!lc_hash_insert(ep->tbl, key, &value, CLIENT)) {
    lc_packet* p = (lc_packet*) value;
    if (p->context.proto & LC_PROTO_DATA) {
      memcpy(src, p->data.buffer, p->context.req->size);
      req->size = p->context.req->size;
      req->flag = 1;
      lc_pool_put(ep->pkpool, p);
    } else {
      p->context.req = req;
      p->context.proto = LC_PROTO_RTR;
      req->size = p->data.rts.size;
      lci_rdz_prepare(ep, src, size, p);
      lc_server_send(ep->hw.handle, tep, &p->data, sizeof(struct packet_rtr), p,
                     LC_PROTO_RTR | LC_PROTO_TAG | LC_PROTO_QUEUE);
    }
  }
  return LC_OK;
}

static inline
lc_status lci_produce(struct lci_ep* ep, lc_wr* wr, lc_req* req)
{
  lc_data* source = &wr->source_data;
  lc_data* target = &wr->target_data;
  // FIXME(danghvu): how do we know gid vs. eid vs. veid.

  // TODO(danghvu): handle more type of source data.
  void* src_buf = source->addr;
  size_t size = source->size;

  switch(target->type) {
    case DAT_EXPL :
    case DAT_TAG :
      return lci_send_tag(ep, wr->target, src_buf, size, target->tag_val, &wr->local_sig, &wr->remote_sig, req);
    case DAT_ALLOC :
      return lci_send_alloc(ep, wr->target, src_buf, size, &wr->local_sig, &wr->remote_sig, req);
    default:
      assert(0 && "Invalid type");
  }
}

static inline
lc_status lci_consume(struct lci_ep* ep, lc_wr* wr, lc_req* req)
{
  lc_data* source = &wr->source_data;
  lc_data* target = &wr->target_data;
  void* tgt_buf = target->addr;
  size_t size = target->size;

  switch(source->type) {
    case DAT_EXPL :
    case DAT_TAG :
      return lci_recv_tag(ep, wr->source, tgt_buf, size, source->tag_val, &wr->local_sig, &wr->remote_sig, req);
    default:
      assert(0 && "Invalid type");
  }

  return LC_OK;
}

static inline
lc_status lci_complete(struct lci_ep* ep, struct lc_sig* sig, lc_req* req)
{
  if (sig->type == SIG_CQ) {
    lc_packet* p = cq_pop(&ep->cq);
    if (!p) return LC_ERR_RETRY;

    memcpy(req, p->context.req, sizeof(struct lc_req));
    lc_pool_put(ep->pkpool, p);
  }
  return LC_OK;
}

lc_status lc_submit(lc_wr* wr, lc_req* req)
{
  struct lci_ep* ep = &g_lc_info.my_ep;
  if (wr->type == WR_PROD) {
    return lci_produce(ep, wr, req);
  } else if (wr->type == WR_CONS) {
    return lci_consume(ep, wr, req);
  }
  return LC_OK;
}

lc_status lc_ce_test(struct lc_sig* sig, lc_req* req)
{
  return lci_complete(&g_lc_info.my_ep, sig, req);
}

lc_status lc_progress()
{
  struct lci_ep* ep = &g_lc_info.my_ep;
  lc_server_progress(ep->hw.handle);
  return LC_OK;
}

lc_status lc_free(void* buf)
{
  g_lc_info.my_ep.free(buf);
  return LC_OK;
}
