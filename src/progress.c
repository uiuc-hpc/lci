#include "mv_priv.h"

extern int PROTO_READY_FIN;
extern int PROTO_SEND_WRITE_FIN;

static void mv_recv_am(mvh* mv, mv_packet* p)
{
  uint8_t fid = (uint8_t)p->header.tag;
  uint32_t* buffer = (uint32_t*)p->content.buffer;
  uint32_t size = buffer[0];
  char* data = (char*)&buffer[1];
  ((_0_arg)mv->am_table[fid])(data, size);
}

static void mv_recv_recv_ready(mvh* mv, mv_packet* p)
{
  mv_key key = mv_make_rdz_key(p->header.from, p->header.tag);
  mv_value value = (mv_value)p;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    proto_complete_rndz(mv, p, (mv_ctx*)value);
  }
}

static void mv_recv_send_ready_fin(mvh* mv, mv_packet* p_ctx)
{
  // Now data is already ready in the content.buffer.
  mv_ctx* req = (mv_ctx*)(p_ctx->content.rdz.rreq);

  mv_key key = mv_make_key(req->rank, req->tag);
  mv_value value = 0;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    req->type = REQ_DONE;
    thread_signal(req->sync);
  }
  mv_pool_put(mv->pkpool, p_ctx);
}

static void mv_recv_short(mvh* mv, mv_packet* p)
{
  const mv_key key = mv_make_key(p->header.from, p->header.tag);
  mv_value value = (mv_value)p;

  if (!mv_hash_insert(mv->tbl, key, &value)) {
    // comm-thread comes later.
    mv_ctx* req = (mv_ctx*)value;
    memcpy(req->buffer, p->content.buffer, req->size);
    req->type = REQ_DONE;
    thread_signal(req->sync);
    mv_pool_put(mv->pkpool, p);
  }
}

void mv_progress_init(mvh* mv)
{
  PROTO_SHORT = mv_am_register(mv, mv_recv_short);
  PROTO_RECV_READY = mv_am_register(mv, (mv_am_func_t)mv_recv_recv_ready);
  PROTO_READY_FIN = mv_am_register(mv, (mv_am_func_t)mv_recv_send_ready_fin);
  PROTO_AM = mv_am_register(mv, (mv_am_func_t)mv_recv_am);
}


void mv_serve_recv(mvh* mv, mv_packet* p_ctx)
{
  const int8_t fid = p_ctx->header.fid;
  ((p_ctx_handler)mv->am_table[fid])(mv, p_ctx);
}

void mv_serve_send(mvh* mv, mv_packet* p_ctx)
{
  if (!p_ctx) return;

  const int8_t fid = p_ctx->header.fid;
  if (unlikely(fid == PROTO_SEND_WRITE_FIN)) {
    mv_ctx* req = (mv_ctx*)p_ctx->content.rdz.sreq;
    p_ctx->header.fid = PROTO_READY_FIN;
    mv_server_send(mv->server, req->rank, p_ctx,
                   sizeof(packet_header) + sizeof(struct mv_rdz), p_ctx);
    mv_key key = mv_make_key(req->rank, (1 << 30) | req->tag);
    mv_value value = 0;
    if (!mv_hash_insert(mv->tbl, key, &value)) {
      req->type = REQ_DONE;
      thread_signal(req->sync);
    }
  } else {
    // NOTE: This improves performance on memcpy, since it sends back
    // the packet to the sender thread. However, this causes a cache misses on the
    // spinlock of the pool. TODO(danghvu): send back only medium msg ?
    mv_pool_put_to(mv->pkpool, p_ctx, p_ctx->header.poolid);
  }
}

void mv_serve_imm(uint32_t imm) { 
  printf("GOT ID %d\n", imm);
}
