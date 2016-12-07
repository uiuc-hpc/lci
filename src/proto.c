#include <mpi.h>
#include "include/mv_priv.h"

/*! Protocol key FIXME */
int PROTO_SHORT;
int PROTO_RECV_READY;
int PROTO_READY_FIN;
int PROTO_AM;
int PROTO_SEND_WRITE_FIN = 99;

void mv_send_rdz_post(mvh* mv, mv_ctx* ctx, mv_sync* sync)
{
  ctx->sync = sync;
  ctx->type = REQ_PENDING;
  mv_key key = mv_make_key(ctx->rank, (1 << 30) | ctx->tag);
  mv_value value = (mv_value) ctx;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    ctx->type = REQ_DONE;
  }
}

void mv_send_rdz_init(mvh* mv, mv_ctx* ctx)
{
  mv_key key = mv_make_rdz_key(ctx->rank, ctx->tag);
  mv_value value = (mv_value)ctx;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    proto_complete_rndz(mv, (mv_packet*)value, ctx);
  }
}

void mv_send_rdz(mvh* mv, mv_ctx* ctx, mv_sync* sync)
{
  mv_send_rdz_init(mv, ctx);
  mv_send_rdz_post(mv, ctx, sync);
  while (ctx->type != REQ_DONE) {
    thread_wait(sync);
  }
}

void mv_send_eager(mvh* mv, mv_ctx* ctx)
{
  // Get from my pool.
  mv_packet* p = (mv_packet*) mv_pool_get(mv->pkpool);
  p->header.fid = PROTO_SHORT;
  p->header.poolid = mv_pool_get_local(mv->pkpool);
  p->header.from = mv->me;
  p->header.tag = ctx->tag;

  // This is a eager message, we send them immediately and do not yield
  // or create a request for it.
  // Copy the buffer.

  memcpy(p->content.buffer, ctx->buffer, ctx->size);
  mv_server_send(mv->server, ctx->rank, (void*)p,
                 (size_t)(ctx->size + sizeof(packet_header)), (void*)(p));
}

void mv_recv_rdz_init(mvh* mv, mv_ctx* ctx)
{
  mv_packet* p = (mv_packet*) mv_pool_get(mv->pkpool); //, 0);
  p->header.fid = PROTO_RECV_READY;
  p->header.poolid = 0;
  p->header.from = mv->me;
  p->header.tag = ctx->tag;

  p->content.rdz.sreq = 0;
  p->content.rdz.rreq = (uintptr_t) ctx;
  p->content.rdz.tgt_addr = (uintptr_t)ctx->buffer;
  p->content.rdz.rkey = mv_server_heap_rkey(mv->server, mv->me);

  mv_server_send(mv->server, ctx->rank, p, sizeof(packet_header) + sizeof(struct mv_rdz),
                 p);
}

void mv_recv_rdz_post(mvh* mv, mv_ctx* ctx, mv_sync* sync)
{
  ctx->sync = sync;
  ctx->type = REQ_PENDING;
  mv_key key = mv_make_key(ctx->rank, ctx->tag);
  mv_value value = 0;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    ctx->type = REQ_DONE;
  }
}

void mv_recv_rdz(mvh* mv, mv_ctx* ctx, mv_sync* sync)
{
  mv_recv_rdz_init(mv, ctx);
  mv_recv_rdz_post(mv, ctx, sync);
  while (ctx->type != REQ_DONE) {
    thread_wait(sync);
  }
}

void mv_recv_eager_post(mvh* mv, mv_ctx* ctx, mv_sync* sync)
{
  ctx->sync = sync;
  ctx->type = REQ_PENDING;
  mv_key key = mv_make_key(ctx->rank, ctx->tag);
  mv_value value = (mv_value)ctx;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    ctx->type = REQ_DONE;
    mv_packet* p_ctx = (mv_packet*)value;
    memcpy(ctx->buffer, p_ctx->content.buffer, ctx->size);
    mv_pool_put(mv->pkpool, p_ctx);
  }
}

void mv_recv_eager(mvh* mv, mv_ctx* ctx, mv_sync* sync)
{
  mv_recv_eager_post(mv, ctx, sync);
  while (ctx->type != REQ_DONE) {
    thread_wait(sync);
  }
}

void mv_am_eager(mvh* mv, int node, void* src, int size,
                           uint32_t fid)
{
  mv_packet* p = (mv_packet*) mv_pool_get(mv->pkpool); 
  p->header.fid = PROTO_AM;
  p->header.from = mv->me;
  p->header.tag = fid;
  uint32_t* buffer = (uint32_t*)p->content.buffer;
  buffer[0] = size;
  memcpy((void*)&buffer[1], src, size);
  mv_server_send(mv->server, node, p,
                 sizeof(uint32_t) + (uint32_t)size + sizeof(packet_header),
                 p);
}

void mv_put(mvh* mv, int node, void* dst, void* src, int size,
                      uint32_t sid)
{
  mv_server_rma_signal(mv->server, node, src, dst,
                       mv_server_heap_rkey(mv->server, node), size, sid, 0);
}
