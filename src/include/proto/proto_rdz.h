#ifndef MV_PROTO_RDZ_H
#define MV_PROTO_RDZ_H

extern uintptr_t mv_comm_id[MAX_COMM_ID];

MV_INLINE void proto_complete_rndz(mvh* mv, mv_packet* p, mv_ctx* ctx)
{
  int rank = p->data.header.from;
  mv_set_proto(p, MV_PROTO_LONG_MATCH);
  p->data.content.rdz.sreq = (uintptr_t)ctx;
  mv_server_rma_signal(mv->server, p->data.header.from, ctx->buffer,
      (void*) p->data.content.rdz.tgt_addr,
      mv_server_heap_rkey(mv->server, rank), ctx->size,
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
  uint64_t comm_idx = (uint64_t) mv_pool_get(mv->idpool);
  mv_comm_id[comm_idx] = (uintptr_t) ctx;
  p->data.content.rdz.sreq = 0;
  p->data.content.rdz.comm_id = (uint32_t) comm_idx;
  p->data.content.rdz.tgt_addr = (uintptr_t)ctx->buffer;
  p->data.content.rdz.rkey = mv_server_heap_rkey(mv->server, mv->me);
  mvi_am_rdz_generic(mv, rank, tag, size, MV_PROTO_RTR_MATCH, p);
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

#endif
