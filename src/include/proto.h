#ifndef MV_PROTO_H
#define MV_PROTO_H

#define RDZ_MATCH_TAG ((uint32_t) 1 << 30)

#define INIT_CTX(ctx)         \
  {                           \
    ctx->buffer = (void*)src; \
    ctx->size = size;         \
    ctx->rank = rank;         \
    ctx->tag = tag;           \
    ctx->type = REQ_PENDING;  \
  }

typedef struct {
  mv_am_func_t func_am;
  mv_am_func_t func_ps;
} mv_proto_spec_t;

const mv_proto_spec_t mv_proto[11] __attribute__((aligned(64)));

MV_INLINE
int mvi_am_generic(mvh* mv, int node, const void* src, int size, int tag,
                   uint32_t proto, mv_packet* p)
{
  if (size > 1024)
    p->context.poolid = mv_pool_get_local(mv->pkpool);
  else
    p->context.poolid = 0;

  p->data.header.from = mv->me;
  p->data.header.tag = tag;
  p->data.header.size = size;
  memcpy(p->data.content.buffer, src, size);
  return mv_server_send(mv->server, node, &p->data,
                        (size_t)(size + sizeof(struct packet_header)),
                        p, proto);
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
    mv_key key = mv_make_key(mv->me, RDZ_MATCH_TAG | ctx->tag);
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
  p->data.content.rdz.sreq = (uintptr_t)ctx;
  mv_server_rma_signal(mv->server, p->data.header.from, ctx->buffer,
                       p->data.content.rdz.tgt_addr, p->data.content.rdz.rkey,
                       ctx->size, p->data.content.rdz.comm_id, p, MV_PROTO_LONG_MATCH);
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
  mv_key key = mv_make_key(ctx->rank, RDZ_MATCH_TAG | ctx->tag);
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
  p->context.req = (uintptr_t)ctx;
  uintptr_t reg = mv_server_rma_reg(mv->server, src, size);

  p->data.header.from = mv->me;
  p->data.header.size = size;
  p->data.header.tag = tag;
  p->data.content.rdz.comm_id =
      (uint32_t)((uintptr_t)p - (uintptr_t)mv_heap_ptr(mv));
  p->data.content.rdz.tgt_addr = (uintptr_t)src;
  p->data.content.rdz.rkey = mv_server_rma_key(reg);

  mv_server_send(mv->server, rank, &p->data,
                 (size_t)(sizeof(struct mv_rdz) + sizeof(struct packet_header)),
                 p, MV_PROTO_RTR_MATCH);
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

MV_INLINE int mvi_send(mvh* mv, const void* src, int size, int rank, int tag, mv_ctx* ctx)
{
  if (size <= (int) SHORT_MSG_SIZE) {
    mv_packet* p = mv_pool_get_nb(mv->pkpool);
    if (!p) return 0;
    mvi_am_generic(mv, rank, src, size, tag, MV_PROTO_SHORT_MATCH, p);
    ctx->type = REQ_DONE;
  } else {
    mvi_send_rdz_init(mv, src, size, rank, tag, ctx);
  }
  return 1;
}

MV_INLINE int mvi_send_post(mvh* mv, mv_ctx* ctx, mv_sync* sync)
{
  if (!ctx || ctx->size <= (int) SHORT_MSG_SIZE)
    return 1;
  else
    return mvi_send_rdz_post(mv, ctx, sync);
}

MV_INLINE int mvi_recv(mvh* mv, void* src, int size, int rank, int tag, mv_ctx* ctx)
{
  if (size <= (int) SHORT_MSG_SIZE)
    mvi_recv_eager_init(mv, src, size, rank, tag, ctx);
  else {
    mv_packet* p = mv_pool_get_nb(mv->pkpool);
    if (!p) return 0;
    mvi_recv_rdz_init(mv, src, size, rank, tag, ctx, p);
  }
  return 1;
}

MV_INLINE int mvi_recv_post(mvh* mv, mv_ctx* ctx, mv_sync* sync)
{
  if (ctx->size <= (int) SHORT_MSG_SIZE)
    return mvi_recv_eager_post(mv, ctx, sync);
  else
    return mvi_recv_rdz_post(mv, ctx, sync);
}

MV_INLINE
void mv_serve_recv(mvh* mv, mv_packet* p_ctx, uint32_t proto)
{
  mv_proto[proto].func_am(mv, p_ctx);
}

MV_INLINE
void mv_serve_send(mvh* mv, mv_packet* p_ctx, uint32_t proto)
{
  if (!p_ctx) return;
  const mv_am_func_t f = mv_proto[proto].func_ps;
  if (likely(f)) {
    f(mv, p_ctx);
  }
}

MV_INLINE
void mv_serve_imm(mvh* mv, uint32_t imm)
{
  // FIXME(danghvu): This comm_id is here due to the imm
  // only takes uint32_t, if this takes uint64_t we can
  // store a pointer to this request context.
  if (imm & RMA_SIGNAL_QUEUE) {
    imm ^= RMA_SIGNAL_QUEUE;
    mv_packet* p = (mv_packet*)((uintptr_t)mv_heap_ptr(mv) + imm);
    mv_ctx* req = (mv_ctx*)p->context.req;
    mv_server_rma_dereg(p->context.rma_mem);
    mv_pool_put(mv->pkpool, p);
    req->type = REQ_DONE;
  } else if (imm & RMA_SIGNAL_SIMPLE) {
    imm ^= RMA_SIGNAL_SIMPLE;
    struct mv_rma_ctx* ctx =
        (struct mv_rma_ctx*)((uintptr_t)mv_heap_ptr(mv) + imm);
    if (ctx->req) ((mv_ctx*)ctx->req)->type = REQ_DONE;
  } else {
    mv_packet* p = (mv_packet*)((uintptr_t)mv_heap_ptr(mv) + imm);
    mv_ctx* req = (mv_ctx*)p->context.req;
    mv_pool_put(mv->pkpool, p);
    mv_key key = mv_make_key(req->rank, req->tag);
    mv_value value = 0;
    if (!mv_hash_insert(mv->tbl, key, &value)) {
      req->type = REQ_DONE;
      if (req->sync) thread_signal(req->sync);
    }
  }
}
#endif
