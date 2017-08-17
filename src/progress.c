#include "lc_priv.h"

#if 0
static void lc_recv_am(lch* mv, lc_packet* p)
{
  uint8_t fid = (uint8_t)p->data.header.tag;
  uint32_t* buffer = (uint32_t*)p->data.buffer;
  uint32_t size = buffer[0];
  char* data = (char*)&buffer[1];
  ((_0_arg)mv->am_table[fid])(data, size);
}
#endif

static void lc_recv_short_match(lch* mv, lc_packet* p)
{
  const lc_key key = lc_make_key(p->context.from, p->context.tag);
  lc_value value = (lc_value)p;

  if (!lc_hash_insert(mv->tbl, key, &value, SERVER)) {
    // data has comes.
    lc_req* req = (lc_req*)value;
    memcpy(req->buffer, p->data.buffer, req->size);
    req->type = REQ_DONE;
    if (req->sync) g_sync.signal(req->sync);
    lc_pool_put(mv->pkpool, p);
  }
}

static void lc_sent_rdz_enqueue_done(lch* mv, lc_packet* p)
{
  lc_req* ctx = (lc_req*) p->context.req;
  ctx->type = REQ_DONE;
  lc_pool_put(mv->pkpool, p);
}

static void lc_recv_rtr_match(lch* mv, lc_packet* p)
{
  lc_key key = lc_make_rdz_key(p->context.from, p->context.tag);
  lc_value value = (lc_value)p;
  if (!lc_hash_insert(mv->tbl, key, &value, SERVER)) {
    lc_req* ctx = (lc_req*)value;
    p->context.req = (uintptr_t)ctx;
    p->context.proto = LC_PROTO_LONG_MATCH;
    lci_put(mv, ctx->buffer, ctx->size, p->context.from,
      p->data.rtr.tgt_addr, p->data.rtr.rkey,
      0, p->data.rtr.comm_id, p);
  }
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

static void lc_sent_rdz_match_done(lch* mv, lc_packet* p)
{
  lc_req* req = (lc_req*) p->context.req;
  req->type = REQ_DONE;
  if (req->sync) g_sync.signal(req->sync);
  lc_pool_put(mv->pkpool, p);
}

static void lc_sent_put(lch* mv, lc_packet* p_ctx)
{
  lc_req* req = (lc_req*) p_ctx->context.req;
  req->type = REQ_DONE;
  lc_pool_put(mv->pkpool, p_ctx);
}

static void lc_sent_persis(lch* mv __UNUSED__, lc_packet* p_ctx)
{
  lc_req* req = (lc_req*) p_ctx->context.req;
  req->type = REQ_DONE;
}

static void lc_sent_done(lch* mv, lc_packet* p_ctx)
{
  if (p_ctx->context.poolid)
    lc_pool_put_to(mv->pkpool, p_ctx, p_ctx->context.poolid);
  else
    lc_pool_put(mv->pkpool, p_ctx);
}

static void lc_recv_queue_packet_short(lch* mv, lc_packet* p)
{
  p->context.proto = LC_PROTO_SHORT_QUEUE;
#ifndef USE_CCQ
  dq_push_top(&mv->queue, (void*) p);
#else
  lcrq_enqueue(&mv->queue, (void*) p);
#endif
}

static void lc_recv_queue_packet_long(lch* mv, lc_packet* p)
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
  {lc_recv_short_match, lc_sent_done},
  {lc_recv_rtr_match, 0},
  {0, lc_sent_rdz_match_done},

  /** Queue Matching protocol */
  {lc_recv_queue_packet_short, lc_sent_done},
  {lc_recv_queue_packet_long, lc_sent_done},
  {lc_recv_rtr_queue, 0},
  {0, lc_sent_rdz_enqueue_done},

  /** Other experimental */
  {0, lc_sent_put},
  {lc_recv_queue_packet_short, lc_sent_persis}, // PERSIS
};
