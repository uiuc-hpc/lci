#ifndef MV_PROTO_EAGER_H
#define MV_PROTO_EAGER_H

MV_INLINE
int mvi_send_eager(mvh* mv, void* src, int size, int rank, int tag)
{
  // Get from my pool.
  mv_packet* p = (mv_packet*)mv_pool_get_nb(mv->pkpool);
  if (unlikely(!p)) {
    do {
      p = mv_pool_get_nb(mv->pkpool);
      thread_yield();
    } while (!p);
  }
  return mvi_am_generic(mv, rank, src, size, tag, MV_PROTO_SHORT, p);
}

MV_INLINE
void mvi_send_eager_post(mvh* mv, mv_ctx* ctx, mv_sync* sync)
{
  int wait = mvi_send_eager(mv, ctx->buffer, ctx->size, ctx->rank, ctx->tag);
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

#endif
