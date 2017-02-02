#ifndef MV_PROTO_RDZ_H
#define MV_PROTO_RDZ_H

MV_INLINE void proto_complete_rndz(mvh* mv, mv_packet* p, mv_ctx* s)
{
  mv_set_proto(p, MV_PROTO_SEND_FIN);
  p->data.header.poolid = 0;
  p->data.header.from = mv->me;
  p->data.header.tag = s->tag;
  p->data.content.rdz.sreq = (uintptr_t)s;

  mv_server_rma(mv->server, s->rank, s->buffer,
                (void*)p->data.content.rdz.tgt_addr, p->data.content.rdz.rkey,
                s->size, &p->context);
}

MV_INLINE void mvi_send_rdz_init(mvh* mv, void* src, int size, int rank,
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
                                 int tag, mv_ctx* ctx)
{
  INIT_CTX(ctx);
  mv_packet* p = (mv_packet*)mv_pool_get(mv->pkpool);
  mv_set_proto(p, MV_PROTO_RECV_READY);
  p->data.header.poolid = 0;
  p->data.header.from = mv->me;
  p->data.header.tag = ctx->tag;

  p->data.content.rdz.sreq = 0;
  p->data.content.rdz.rreq = (uintptr_t)ctx;
  p->data.content.rdz.tgt_addr = (uintptr_t)ctx->buffer;
  p->data.content.rdz.rkey = mv_server_heap_rkey(mv->server, mv->me);

  mv_server_send(mv->server, ctx->rank, &p->data,
                 sizeof(packet_header) + sizeof(struct mv_rdz), &p->context);
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
