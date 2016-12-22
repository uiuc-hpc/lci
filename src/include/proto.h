#ifndef MV_PROTO_H_
#define MV_PROTO_H_

/*! Protocol key FIXME */
#define PROTO_SHORT 0
#define PROTO_RECV_READY 1
#define PROTO_READY_FIN 2
#define PROTO_AM 3
#define PROTO_SHORT_WAIT 4

#define PROTO_SEND_WRITE_FIN 127

extern void mv_server_rma(mv_server* s, int rank, void* from,
        void* to, uint32_t rkey, size_t size,
        void* ctx);

MV_INLINE void proto_complete_rndz(mvh* mv, mv_packet* p, mv_ctx* s)
{
  p->data.header.fid = PROTO_SEND_WRITE_FIN;
  p->data.header.poolid = 0;
  p->data.header.from = mv->me;
  p->data.header.tag = s->tag;
  p->data.content.rdz.sreq = (uintptr_t)s;

  mv_server_rma(mv->server, s->rank, s->buffer, (void*)p->data.content.rdz.tgt_addr,
                p->data.content.rdz.rkey, s->size, &p->context);
}

MV_INLINE void mvi_wait(mv_ctx *ctx, mv_sync* sync)
{
  while (ctx->type != REQ_DONE) {
    thread_wait(sync);
  }
}

MV_INLINE void mvi_send_eager(mvh* mv, mv_ctx* ctx)
{
  // Get from my pool.
  mv_packet* p = (mv_packet*) mv_pool_get_nb(mv->pkpool);
  if (unlikely(!p)) {
    do {
      p = mv_pool_get_nb(mv->pkpool);
      thread_yield();
    } while (!p);
  }
  p->data.header.fid = PROTO_SHORT;
  p->data.header.poolid = mv_pool_get_local(mv->pkpool);
  p->data.header.from = mv->me;
  p->data.header.tag = ctx->tag;

  // This is a eager message, we send them immediately and do not yield
  // or create a request for it.
  // Copy the buffer.
  memcpy(p->data.content.buffer, ctx->buffer, ctx->size);

  mv_server_send(mv->server, ctx->rank, &p->data,
                 (size_t)(ctx->size + sizeof(packet_header)), &p->context);
}

MV_INLINE void mvi_send_eager_post(mvh* mv, mv_ctx* ctx, mv_sync* sync)
{
  // Get from my pool.
  mv_packet* p = (mv_packet*) mv_pool_get_nb(mv->pkpool);
  if (unlikely(!p)) {
    do {
      p = mv_pool_get_nb(mv->pkpool);
      thread_yield();
    } while (!p);
  }

  p->data.header.fid = PROTO_SHORT_WAIT;
  p->data.header.poolid = mv_pool_get_local(mv->pkpool);
  p->data.header.from = mv->me;
  p->data.header.tag = ctx->tag;

  // This is a eager message, we send them immediately and do not yield
  // or create a request for it.
  // Copy the buffer.
  memcpy(p->data.content.buffer, ctx->buffer, ctx->size);

  int wait = mv_server_send(mv->server, ctx->rank, &p->data,
      (size_t)(ctx->size + sizeof(packet_header)), &p->context);
  
  if (wait) {
    mv_key key = mv_make_key(mv->me, (1 << 30) | ctx->tag);
    ctx->sync = sync;
    ctx->type = REQ_PENDING;
    mv_value value = (mv_value) ctx;
    if (!mv_hash_insert(mv->tbl, key, &value)) {
      ctx->type = REQ_DONE;
    }
  } else {
    ctx->type = REQ_DONE;
  }
}

MV_INLINE int mvi_send_rdz_post(mvh* mv, mv_ctx* ctx, mv_sync* sync)
{
  ctx->sync = sync;
  ctx->type = REQ_PENDING;
  mv_key key = mv_make_key(ctx->rank, (1 << 30) | ctx->tag);
  mv_value value = (mv_value) ctx;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    ctx->type = REQ_DONE;
    return 1;
  }
  return 0;
}

MV_INLINE void mvi_send_rdz_init(mvh* mv, mv_ctx* ctx)
{
  mv_key key = mv_make_rdz_key(ctx->rank, ctx->tag);
  mv_value value = (mv_value)ctx;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    proto_complete_rndz(mv, (mv_packet*)value, ctx);
  }
}

MV_INLINE void mvi_recv_rdz_init(mvh* mv, mv_ctx* ctx)
{
  mv_packet* p = (mv_packet*) mv_pool_get(mv->pkpool); //, 0);
  p->data.header.fid = PROTO_RECV_READY;
  p->data.header.poolid = 0;
  p->data.header.from = mv->me;
  p->data.header.tag = ctx->tag;

  p->data.content.rdz.sreq = 0;
  p->data.content.rdz.rreq = (uintptr_t) ctx;
  p->data.content.rdz.tgt_addr = (uintptr_t)ctx->buffer;
  p->data.content.rdz.rkey = mv_server_heap_rkey(mv->server, mv->me);

  mv_server_send(mv->server, ctx->rank, &p->data, sizeof(packet_header) + sizeof(struct mv_rdz),
                 &p->context);
}

MV_INLINE int mvi_recv_rdz_post(mvh* mv, mv_ctx* ctx, mv_sync* sync)
{
  ctx->sync = sync;
  ctx->type = REQ_PENDING;
  mv_key key = mv_make_key(ctx->rank, ctx->tag);
  mv_value value = 0;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    ctx->type = REQ_DONE;
    return 1;
  }
  return 0;
}

MV_INLINE int mvi_recv_eager_post(mvh* mv, mv_ctx* ctx, mv_sync* sync)
{
  ctx->sync = sync;
  ctx->type = REQ_PENDING;
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

#endif
