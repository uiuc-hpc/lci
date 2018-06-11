#include "lc.h"

#include "lc_priv.h"
#include "lc/pool.h"

#include <assert.h>

struct lci_hw hw[1];

int lc_current_id = 1;
__thread int lc_core_id = 0;
int server_deadlock_alert;

lc_status lc_init(lc_ep* ep, long cap)
{
  lci_hw_init(&hw[0], cap);
  *ep = &hw[0].master_ep;
  return LC_OK;
}

lc_status lc_finalize(lc_ep ep)
{
  return LC_OK;
}

lc_id lc_rank(lc_ep ep)
{
  return ep->eid;
}

static inline
lc_status lci_send_alloc(struct lci_ep *ep, lc_id tep, void* src, size_t size, lc_meta meta, lc_req* req)
{
  LC_POOL_GET_OR_RETN(ep->pkpool, p);

  if (size <= (int) SHORT_MSG_SIZE) {
    lc_server_send(ep->hw, tep, src, size, p, MAKE_PROTO(LC_PROTO_DATA | LC_PROTO_QALLOC, meta.val));
    req->flag = 1;
  } else {
    INIT_CTX(req);
    p->data.rts.req = (uintptr_t) req;
    p->data.rts.src_addr = (uintptr_t) src;
    p->data.rts.size = size;
    lc_server_send(ep->hw, tep, &p->data, sizeof(struct packet_rts), p, MAKE_PROTO(LC_PROTO_RTS | LC_PROTO_QALLOC, meta.val));
  }
  return LC_OK;
}

static inline
lc_status lci_send_piggy(struct lci_ep *ep, lc_id tep, void* src, size_t size, lc_meta meta, lc_req* req)
{
  LC_POOL_GET_OR_RETN(ep->pkpool, p);

  if (size <= (int) SHORT_MSG_SIZE) {
    lc_server_send(ep->hw, tep, src, size, p, MAKE_PROTO(LC_PROTO_DATA | LC_PROTO_QSHORT, meta.val));
    req->flag = 1;
  } else {
    assert(0);
  }
  return LC_OK;
}
static inline
lc_status lci_send_tag(struct lci_ep *ep, lc_id tep, void* src, size_t size, lc_meta tag, lc_req* req)
{
  LC_POOL_GET_OR_RETN(ep->pkpool, p);
  if (size <= (int) SHORT_MSG_SIZE) {
    lc_server_send(ep->hw, tep, src, size, p,
                   MAKE_PROTO(LC_PROTO_DATA | LC_PROTO_TAG, tag.val));
    req->flag = 1;
  } else {
    INIT_CTX(req);
    p->data.rts.req = (uintptr_t) req;
    p->data.rts.src_addr = (uintptr_t) src;
    p->data.rts.size = size;
    lc_server_send(ep->hw, tep, &p->data, sizeof(struct packet_rts), p,
                   MAKE_PROTO(LC_PROTO_RTS | LC_PROTO_TAG, tag.val));
  }
  return LC_OK;
}

static inline
lc_status lci_recv_tag(struct lci_ep *ep, lc_id tep, void* src, size_t size, lc_meta tag, lc_req* req)
{
  INIT_CTX(req);
  lc_key key = lc_make_key(tep, tag.val);
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
      lc_server_send(ep->hw, tep, &p->data, sizeof(struct packet_rtr), p,
                     LC_PROTO_RTR | LC_PROTO_TAG);
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
      return lci_send_tag(ep, wr->target, src_buf, size, wr->meta, req);
    case DAT_ALLOC :
      return lci_send_alloc(ep, wr->target, src_buf, size, wr->meta, req);
    case DAT_PIGGY :
      return lci_send_piggy(ep, wr->target, src_buf, size, wr->meta, req);
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
      return lci_recv_tag(ep, wr->source, tgt_buf, size, wr->meta, req);
    default:
      assert(0 && "Invalid type");
  }

  return LC_OK;
}

lc_status lc_submit(lc_ep ep, lc_wr* wr, lc_req* req)
{
  if (wr->type == WR_PROD) {
    return lci_produce(ep, wr, req);
  } else if (wr->type == WR_CONS) {
    return lci_consume(ep, wr, req);
  }
  return LC_OK;
}

lc_status lc_deq_alloc(lc_ep ep, lc_req* req)
{
  lc_packet* p = cq_pop(&ep->cq);
  if (!p) return LC_ERR_RETRY;
  memcpy(req, p->context.req, sizeof(struct lc_req));
  lc_pool_put(ep->pkpool, p);
  return LC_OK;
}

lc_status lc_deq_piggy(lc_ep ep, lc_req** req)
{
  lc_packet* p = cq_pop(&ep->cq);
  if (!p) return LC_ERR_RETRY;
  *req = p->context.req;
  return LC_OK;
}

lc_status lc_req_free(lc_ep ep, lc_req* req)
{
  lc_pool_put(ep->pkpool, req->parent);
  return LC_OK;
}

lc_status lc_progress_t() // TODO: make a version with index.
{
  lc_server_progress(hw[0].handle, EP_TYPE_TAG);
  return LC_OK;
}

lc_status lc_progress_q() // TODO: make a version with index.
{
  lc_server_progress(hw[0].handle, EP_TYPE_QUEUE);
  return LC_OK;
}

lc_status lc_progress_sq() // TODO: make a version with index.
{
  lc_server_progress(hw[0].handle, EP_TYPE_SQUEUE);
  return LC_OK;
}

lc_status lc_progress()
{
  lc_server_progress(hw[0].handle, hw[0].cap);
  return LC_OK;
}

lc_status lc_free(lc_ep ep, void* buf)
{
  ep->free(buf);
  return LC_OK;
}
