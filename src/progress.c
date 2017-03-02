#include "mv_priv.h"

static void mv_recv_am(mvh* mv, mv_packet* p)
{
  uint8_t fid = (uint8_t)p->data.header.tag;
  uint32_t* buffer = (uint32_t*)p->data.content.buffer;
  uint32_t size = buffer[0];
  char* data = (char*)&buffer[1];
  ((_0_arg)mv->am_table[fid])(data, size);
}

static void mv_recv_short_match(mvh* mv, mv_packet* p)
{
  const mv_key key = mv_make_key(p->data.header.from, p->data.header.tag);
  mv_value value = (mv_value)p;

  if (!mv_hash_insert(mv->tbl, key, &value)) {
    // comm-thread comes later.
    mv_ctx* req = (mv_ctx*)value;
    memcpy(req->buffer, p->data.content.buffer, req->size);
    req->type = REQ_DONE;
    if (req->sync) thread_signal(req->sync);
    mv_pool_put(mv->pkpool, p);
  }
}

static void mv_recv_short_enqueue(mvh* mv, mv_packet* p)
{
#ifndef USE_CCQ
  dq_push_top(&mv->queue, (void*) p);
#else
  lcrq_enqueue(&mv->queue, (void*) p);
#endif
}

static void mv_sent_rdz_enqueue_done(mvh* mv, mv_packet* p)
{
  mv_ctx* ctx = (mv_ctx*) p->data.content.rdz.sreq;
  ctx->type = REQ_DONE;
  mv_pool_put(mv->pkpool, p);
}

static void mv_recv_rtr_match(mvh* mv, mv_packet* p)
{
  mv_key key = mv_make_rdz_key(p->data.header.from, p->data.header.tag);
  mv_value value = (mv_value)p;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    proto_complete_rndz(mv, p, (mv_ctx*)value);
  }
}

static void mv_recv_rtr_queue(mvh* mv, mv_packet* p)
{
  mv_ctx* ctx = (mv_ctx*) p->data.content.rdz.sreq;
  int rank = ctx->rank;
  p->data.header.proto = MV_PROTO_LONG_ENQUEUE;
  mv_server_rma_signal(mv->server, rank, ctx->buffer,
      p->data.content.rdz.tgt_addr,
      p->data.content.rdz.rkey,
      ctx->size,
      RMA_SIGNAL_QUEUE | (p->data.content.rdz.comm_id), &p->context);
}

static void mv_recv_rts_queue(mvh* mv, mv_packet* p)
{
#if 1
#ifndef USE_CCQ
  dq_push_top(&mv->queue, (void*) p);
#else
  lcrq_enqueue(&mv->queue, (void*) p);
#endif
#else
    int rank = p->data.header.from;
    // p->context.req = 0; //(uintptr_t) ctx;
    // p->data.header.from = mv->me;
    p->data.header.proto = MV_PROTO_RTR_ENQUEUE;
    void* buf = malloc(p->data.header.size);
    p->data.content.rdz.tgt_addr = (uintptr_t) buf;
    p->data.content.rdz.mem = get_dma_mem(mv->server, buf, p->data.header.size);
    p->data.content.rdz.rkey = fi_mr_key(p->data.content.rdz.mem);
    p->data.content.rdz.comm_id = (uint32_t) ((uintptr_t) p - (uintptr_t) mv_heap_ptr(mv));
    mv_server_send(mv->server, rank, &p->data,
        sizeof(struct packet_header) + sizeof(struct mv_rdz), &p->context);
#endif
}

static void mv_sent_rdz_match_done(mvh* mv, mv_packet* p)
{
  mv_ctx* req = (mv_ctx*) p->data.content.rdz.sreq;
  mv_key key = mv_make_key(req->rank, (1 << 30) | req->tag);
  mv_value value = 0;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    req->type = REQ_DONE;
    if (req->sync) thread_signal(req->sync);
  }
  mv_pool_put(mv->pkpool, p);
}

static void mv_sent_short_wait(mvh* mv, mv_packet* p_ctx)
{
  mv_key key = mv_make_key(mv->me, (1 << 30) | p_ctx->data.header.tag);
  mv_value value = 0;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    mv_ctx* req = (mv_ctx*) value;
    req->type = REQ_DONE;
    if (req->sync) thread_signal(req->sync);
  }
  mv_pool_put_to(mv->pkpool, p_ctx, p_ctx->context.poolid);
}

static void mv_sent_put(mvh* mv, mv_packet* p_ctx)
{
  mv_ctx* req = (mv_ctx*) p_ctx->context.req;
  req->type = REQ_DONE;
  mv_pool_put(mv->pkpool, p_ctx);
}

const mv_proto_spec_t mv_proto[11] = {
  {0, 0}, // Reserved for doing nothing.
  {mv_recv_short_match, MV_PROTO_DONE},
  {mv_recv_short_match, mv_sent_short_wait},
  {mv_recv_rtr_match, 0},
  {0, mv_sent_rdz_match_done},
  {mv_recv_am, MV_PROTO_DONE},
  {mv_recv_short_enqueue, MV_PROTO_DONE},
  {mv_recv_rts_queue, MV_PROTO_DONE},
  {mv_recv_rtr_queue, 0},
  {0, mv_sent_rdz_enqueue_done},
  {0, mv_sent_put},
};


//FIXME(danghvu): Experimental stuff to work-around memory registration.
#ifdef SERVER_IBV_H
uintptr_t get_dma_mem(void* server, void* buf, size_t s)
{
  return _real_ibv_reg((mv_server*) server, buf, s);
}

int free_dma_mem(uintptr_t mem)
{
  return ibv_dereg_mr((struct ibv_mr*) mem);
}
#else
#if 1
uintptr_t get_dma_mem(void* server, void* buf, size_t s)
{
  return _real_ofi_reg((mv_server*) server, buf, s);
}

int free_dma_mem(uintptr_t mem)
{
  return fi_close((struct fid*) mem);
}
#endif
#endif
