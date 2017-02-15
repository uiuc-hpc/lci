#ifndef MV_PROTO_H
#define MV_PROTO_H

#define INIT_CTX(ctx)        \
  {                          \
    ctx->buffer = (void*)src;\
    ctx->size = size;        \
    ctx->rank = rank;        \
    ctx->tag = tag;          \
    ctx->type = REQ_PENDING; \
  }

#define MV_PROTO_DONE ((mv_am_func_t) -1)

typedef struct {
  mv_am_func_t func_am;
  mv_am_func_t func_ps;
} mv_proto_spec_t;

const mv_proto_spec_t mv_proto[10] __attribute__((aligned(64)));
extern uintptr_t mv_comm_id[MAX_COMM_ID];

MV_INLINE
int mvi_am_generic(mvh* mv, int node, const void* src, int size, int tag,
                   const enum mv_proto_name proto, mv_packet* p)
{
  p->context.poolid = mv_pool_get_local(mv->pkpool);
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
  int rank = p->data.header.from;
  p->data.header.proto = MV_PROTO_LONG_MATCH;
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
  uint64_t comm_idx = p->context.pid;
  mv_comm_id[comm_idx] = (uintptr_t) ctx;
  ctx->packet = p;
  p->context.poolid = mv_pool_get_local(mv->pkpool);
  p->data.header.from = mv->me;
  p->data.header.size = size;
  p->data.header.tag = tag;
  p->data.header.proto = MV_PROTO_RTR_MATCH;
  p->data.content.rdz.sreq = 0;
  p->data.content.rdz.comm_id = (uint32_t) comm_idx;
  p->data.content.rdz.tgt_addr = (uintptr_t)ctx->buffer;
  p->data.content.rdz.rkey = mv_server_heap_rkey(mv->server, mv->me);
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
  enum mv_proto_name proto = p_ctx->data.header.proto;
  assert(proto >= 0 && proto <= 10);
  mv_proto[proto].func_am(mv, p_ctx);
}

MV_INLINE
void mv_serve_send(mvh* mv, mv_packet* p_ctx)
{
  if (!p_ctx) return;
  enum mv_proto_name proto = p_ctx->data.header.proto;
  if (mv_proto[proto].func_ps == MV_PROTO_DONE)
    mv_pool_put_to(mv->pkpool, p_ctx, p_ctx->context.poolid);
  else if (mv_proto[proto].func_ps)
    mv_proto[proto].func_ps(mv, p_ctx);
}

MV_INLINE
void mv_serve_imm(mvh* mv, uint32_t imm) {
  // FIXME(danghvu): This comm_id is here due to the imm
  // only takes uint32_t, if this takes uint64_t we can
  // store a pointer to this request context.
  uint32_t real_imm = imm << 2 >> 2;
  // Match + Signal
  mv_ctx* req = (mv_ctx*) mv_comm_id[real_imm];
  if (real_imm == imm) {
    mv_pool_put_to(mv->pkpool, req->packet, req->packet->context.poolid);
    mv_key key = mv_make_key(req->rank, req->tag);
    mv_value value = 0;
    if (!mv_hash_insert(mv->tbl, key, &value)) {
      req->type = REQ_DONE;
      if (req->sync) thread_signal(req->sync);
    }
  } else {
    mv_pool_put_to(mv->pkpool, req->packet, req->packet->context.poolid);
    req->type = REQ_DONE;
  }
}
#endif
