#ifndef MV_PROTO_H
#define MV_PROTO_H

// #include "dreg/dreg.h"

#define INIT_CTX(ctx)        \
  {                          \
    ctx->buffer = (void*)src;\
    ctx->size = size;        \
    ctx->rank = rank;        \
    ctx->tag = tag;          \
    ctx->type = REQ_PENDING; \
  }

#define MV_PROTO_DONE ((mv_am_func_t) -1)
#define MV_PROTO_DATA_DONE ((mv_am_func_t) -2)

typedef struct {
  mv_am_func_t func_am;
  mv_am_func_t func_ps;
} mv_proto_spec_t;

const mv_proto_spec_t mv_proto[10] __attribute__((aligned(64)));

MV_INLINE
int mvi_am_generic(mvh* mv, int node, const void* src, int size, int tag,
                   const enum mv_proto_name proto, mv_packet* p)
{
  // NOTE: need locality here, since this pkt has a lot of data.
  if (size < 4096)
    p->context.poolid = mv_pool_get_local(mv->pkpool);
  else
    p->context.poolid = 0;

  p->data.header.proto = proto;
  p->data.header.from = mv->me;
  p->data.header.tag = tag;
  p->data.header.size = size;
  memcpy(p->data.content.buffer, src, size);
  return mv_server_send(mv->server, node, &p->data,
                        (size_t)(size + sizeof(struct packet_header)), &p->context);
}

MV_INLINE
void mvi_recv_eager_init(mvh* mv __UNUSED__, void* src, int size, int rank,
                         int tag, mv_ctx* ctx)
{
  INIT_CTX(ctx);
}

MV_INLINE
int mvi_recv_eager_post(mvh* mv, mv_ctx* ctx, mv_sync* sync)
{
  ctx->sync = sync;
  mv_key key = mv_make_key(ctx->rank, ctx->tag);
  mv_value value = (mv_value)ctx;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    ctx->type = REQ_DONE;
    mv_packet* p_ctx = (mv_packet*)value;
    memcpy(ctx->buffer, p_ctx->data.content.buffer, ctx->size);
    mv_pool_put(mv->pkpool, p_ctx);
    return 1;
  }
  return 0;
}

MV_INLINE
void mvi_send_eager_post(mvh* mv, mv_ctx* ctx, mv_sync* sync)
{
  // FIXME(danghvu)
  // int wait = mvi_send_eager(mv, ctx->buffer, ctx->size, ctx->rank, ctx->tag);
  int wait = 1;
  if (wait) {
    mv_key key = mv_make_key(mv->me, (1 << 30) | ctx->tag);
    ctx->sync = sync;
    ctx->type = REQ_PENDING;
    mv_value value = (mv_value)ctx;
    if (!mv_hash_insert(mv->tbl, key, &value)) {
      ctx->type = REQ_DONE;
    }
  } else {
    ctx->type = REQ_DONE;
  }
}

MV_INLINE void proto_complete_rndz(mvh* mv, mv_packet* p, mv_ctx* ctx)
{
  p->data.header.proto = MV_PROTO_LONG_MATCH;
  p->data.content.rdz.sreq = (uintptr_t)ctx;
  mv_server_rma_signal(mv->server, p->data.header.from, ctx->buffer,
      p->data.content.rdz.tgt_addr,
      p->data.content.rdz.rkey, ctx->size,
      p->data.content.rdz.comm_id, p);
}

MV_INLINE void mvi_send_rdz_init(mvh* mv, const void* src, int size, int rank,
                                 int tag, mv_ctx* ctx)
{
  INIT_CTX(ctx);
  mv_key key = mv_make_rdz_key(ctx->rank, ctx->tag);
  mv_value value = (mv_value)ctx;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    proto_complete_rndz(mv, (mv_packet*)value, ctx);
  }
}

MV_INLINE int mvi_send_rdz_post(mvh* mv, mv_ctx* ctx, mv_sync* sync)
{
  ctx->sync = sync;
  mv_key key = mv_make_key(ctx->rank, (1 << 30) | ctx->tag);
  mv_value value = (mv_value)ctx;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    ctx->type = REQ_DONE;
    return 1;
  }
  return 0;
}

MV_INLINE void mvi_recv_rdz_init(mvh* mv, void* src, int size, int rank,
                                 int tag, mv_ctx* ctx, mv_packet* p)
{
  INIT_CTX(ctx);
  p->context.req = (uintptr_t) ctx;
  p->context.dma_mem = mv_server_dma_reg(mv->server, src, size);

  p->data.header.from = mv->me;
  p->data.header.size = size;
  p->data.header.tag = tag;
  p->data.header.proto = MV_PROTO_RTR_MATCH;
  p->data.content.rdz.comm_id = (uint32_t) ((uintptr_t) p - (uintptr_t) mv_heap_ptr(mv));
  p->data.content.rdz.tgt_addr = (uintptr_t) src;
  p->data.content.rdz.rkey = mv_server_dma_key(p->context.dma_mem);

  mv_server_send(mv->server, rank, &p->data,
        (size_t)(sizeof(struct mv_rdz) + sizeof(struct packet_header)),
        &p->context);
}

MV_INLINE int mvi_recv_rdz_post(mvh* mv, mv_ctx* ctx, mv_sync* sync)
{
  ctx->sync = sync;
  mv_key key = mv_make_key(ctx->rank, ctx->tag);
  mv_value value = 0;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    ctx->type = REQ_DONE;
    return 1;
  }
  return 0;
}

MV_INLINE
void mv_serve_recv(mvh* mv, mv_packet* p_ctx)
{
  const enum mv_proto_name proto = p_ctx->data.header.proto;
  mv_proto[proto].func_am(mv, p_ctx);
}

MV_INLINE
void mv_serve_send(mvh* mv, mv_packet* p_ctx)
{
  if (!p_ctx) return;
  const enum mv_proto_name proto = p_ctx->data.header.proto;
  const mv_am_func_t f = mv_proto[proto].func_ps;

  if (f == MV_PROTO_DONE) {
    mv_pool_put_to(mv->pkpool, p_ctx, p_ctx->context.poolid);
  } else if (f) {
    f(mv, p_ctx);
  }
}

MV_INLINE
void mv_serve_imm(mvh* mv, uint32_t imm) {
  // FIXME(danghvu): This comm_id is here due to the imm
  // only takes uint32_t, if this takes uint64_t we can
  // store a pointer to this request context.
  uint32_t real_imm = imm << 2 >> 2;
  mv_packet* p = (mv_packet*) ((uintptr_t) mv_heap_ptr(mv) + real_imm);
  // Match + Signal
  if (real_imm == imm) {
    mv_ctx* req = (mv_ctx*) p->context.req;
    mv_server_dma_dereg(p->context.dma_mem);
    mv_pool_put(mv->pkpool, p);
    mv_key key = mv_make_key(req->rank, req->tag);
    mv_value value = 0;
    if (!mv_hash_insert(mv->tbl, key, &value)) {
      req->type = REQ_DONE;
      if (req->sync) thread_signal(req->sync);
    }
  } else {
#if 0
#ifndef USE_CCQ
    dq_push_top(&mv->queue, (void*) p);
#else
    lcrq_enqueue(&mv->queue, (void*) p);
#endif
#else
    mv_ctx* req = (mv_ctx*) p->context.req;
    mv_server_dma_dereg(p->context.dma_mem);
    mv_pool_put(mv->pkpool, p);
    req->type = REQ_DONE;
#endif
  }
}
#endif
