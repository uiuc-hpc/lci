#include "lc_priv.h"

static void lc_recv_tag_short(lch* mv, lc_packet* p)
{
  const lc_key key = lc_make_key(p->context.from, p->context.tag);
  lc_value value = (lc_value)p;

  if (!lc_hash_insert(mv->tbl, key, &value, SERVER)) {
    // data has comes.
    lc_req* req = (lc_req*)value;
    memcpy(req->buffer, p->data.buffer, req->size);
    LC_SET_REQ_DONE_AND_SIGNAL(req);
    lc_pool_put(mv->pkpool, p);
  }
}

static void lc_recv_tag_rts(lch* mv, lc_packet* p)
{
  const lc_key key = lc_make_key(p->context.from, p->context.tag);
  lc_value value = (lc_value)p;

  if (!lc_hash_insert(mv->tbl, key, &value, SERVER)) {
    // data has comes.
    lc_req* req = (lc_req*)value;
    p->context.proto = LC_PROTO_RTR_TAG;
    lci_rdz_prepare(mv, req->buffer, req->size, req, p);
    lci_send(mv, &p->data, sizeof(struct packet_rtr),
        req->rank, req->tag, p);
  }
}

static void lc_sent_rdz_done(lch* mv, lc_packet* p)
{
  lc_req* req = (lc_req*) p->context.req;
  LC_SET_REQ_DONE_AND_SIGNAL(req);
  lc_pool_put(mv->pkpool, p);
}

static void lc_recv_rtr_tag(lch* mv, lc_packet* p)
{
  lc_req* ctx = (lc_req*) p->data.rtr.sreq;
  p->context.req = (uintptr_t)ctx;
  p->context.proto = LC_PROTO_LONG_TAG;
  lci_put(mv, ctx->buffer, ctx->size, p->context.from,
      p->data.rtr.tgt_addr, p->data.rtr.rkey,
      0, p->data.rtr.comm_id, p);
}

static void lc_recv_rtr_queue(lch* mv, lc_packet* p)
{
  p->context.req = (uintptr_t) p->data.rtr.sreq;
  p->context.proto = LC_PROTO_LONG_QUEUE;
  lc_req* ctx = (lc_req*) p->data.rtr.sreq;
  lci_put(mv, ctx->buffer, ctx->size, p->context.from,
      p->data.rtr.tgt_addr, p->data.rtr.rkey,
      RMA_SIGNAL_QUEUE, p->data.rtr.comm_id, p);
}

static void lc_sent_put(lch* mv, lc_packet* p_ctx)
{
  lc_req* req = (lc_req*) p_ctx->context.req;
  LC_SET_REQ_DONE_AND_SIGNAL(req);
  lc_pool_put(mv->pkpool, p_ctx);
}

static void lc_sent_done(lch* mv, lc_packet* p_ctx)
{
  if (p_ctx->context.runtime) {
    if (p_ctx->context.poolid)
      lc_pool_put_to(mv->pkpool, p_ctx, p_ctx->context.poolid);
    else
      lc_pool_put(mv->pkpool, p_ctx);
  }
}

static void lc_recv_queue_short(lch* mv, lc_packet* p)
{
  p->context.proto = LC_PROTO_SHORT_QUEUE;
#ifndef USE_CCQ
  dq_push_top(&mv->queue, (void*) p);
#else
  lcrq_enqueue(&mv->queue, (void*) p);
#endif
}

static void lc_recv_queue_rts(lch* mv, lc_packet* p)
{
  p->context.proto = LC_PROTO_RTS_QUEUE;
#ifndef USE_CCQ
  dq_push_top(&mv->queue, (void*) p);
#else
  lcrq_enqueue(&mv->queue, (void*) p);
#endif
}

uintptr_t get_dma_mem(void* server, void* buf, size_t s)
{
  return _real_server_reg((lc_server*) server, buf, s);
}

int free_dma_mem(uintptr_t mem)
{
  _real_server_dereg(mem);
  return 1;
}

const lc_proto_spec_t lc_proto[10] = {
  {0, 0}, // Reserved for doing nothing.

  /** Tag Matching protocol */
  {lc_recv_tag_short, lc_sent_done},
  {lc_recv_tag_rts, lc_sent_done},
  {lc_recv_rtr_tag, 0},
  {0, lc_sent_rdz_done},

  /** Queue Matching protocol */
  {lc_recv_queue_short, lc_sent_done},
  {lc_recv_queue_rts, lc_sent_done},
  {lc_recv_rtr_queue, 0},
  {0, lc_sent_rdz_done},

  /** Other experimental */
  {0, lc_sent_put},
};
