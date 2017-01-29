#include "mv_priv.h"

static void mv_recv_am(mvh* mv, mv_packet* p)
{
  uint8_t fid = (uint8_t)p->data.header.tag;
  uint32_t* buffer = (uint32_t*)p->data.content.buffer;
  uint32_t size = buffer[0];
  char* data = (char*)&buffer[1];
  ((_0_arg)mv->am_table[fid])(data, size);
}

static void mv_recv_recv_ready(mvh* mv, mv_packet* p)
{
  mv_key key = mv_make_rdz_key(p->data.header.from, p->data.header.tag);
  mv_value value = (mv_value)p;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    proto_complete_rndz(mv, p, (mv_ctx*)value);
  }
}

static void mv_recv_send_ready_fin(mvh* mv, mv_packet* p_ctx)
{
  // Now data is already ready in the content.buffer.
  mv_ctx* req = (mv_ctx*)(p_ctx->data.content.rdz.rreq);

  mv_key key = mv_make_key(req->rank, req->tag);
  mv_value value = 0;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    req->type = REQ_DONE;
    if (req->sync) thread_signal(req->sync);
  }
  mv_pool_put(mv->pkpool, p_ctx);
}

static void mv_recv_short(mvh* mv, mv_packet* p)
{
  const mv_key key = mv_make_key(p->data.header.from, p->data.header.tag);
  mv_value value = (mv_value)p;

  if (!mv_hash_insert(mv->tbl, key, &value)) {
    // comm-thread comes later.
    mv_ctx* req = (mv_ctx*)value;
    memcpy(req->buffer, p->data.content.buffer, req->size);
    req->type = REQ_DONE;
    if (req->sync) thread_signal(req->sync);
    if (mv->server->recv_posted < MAX_RECV)
      mv_server_post_recv(mv->server, p);
    else
      mv_pool_put(mv->pkpool, p);
  }
}

static void mv_recv_short_enqueue(mvh* mv, mv_packet* p)
{
  dq_push_top(&mv->queue, (void*) p); 
}

static void mv_sent_rdz_enqueue_done(mvh* mv, mv_packet* p)
{
  mv_ctx* ctx = (mv_ctx*) p->data.content.rdz.sreq;
  ctx->type = REQ_DONE;
  mv_pool_put(mv->pkpool, p);
}

static void mv_recv_rdz_enqueue_done(mvh* mv, mv_packet* p)
{
  dq_push_top(&mv->queue, (void*) p); 
}

static void mv_recv_rtr(mvh* mv, mv_packet* p)
{
  mv_ctx* ctx = (mv_ctx*) p->data.content.rdz.sreq;
  mv_put(mv, p->data.header.from, (void*) p->data.content.rdz.tgt_addr,
      ctx->buffer, ctx->size);
  int rank = p->data.header.from;
  p->data.header.from = mv->me;
  mv_set_proto(p, MV_PROTO_LONG_ENQUEUE);
  mv_server_send(mv->server, rank, &p->data,
      sizeof(packet_header) + sizeof(struct mv_rdz), &p->context);
}

static void mv_recv_rts(mvh* mv, mv_packet* p)
{
  void* ptr = mv_alloc(mv, p->data.header.size);
  int rank = p->data.header.from;
  p->data.header.from = mv->me;
  mv_set_proto(p, MV_PROTO_RTR);
  p->data.content.rdz.tgt_addr = (uintptr_t) ptr;
  mv_server_send(mv->server, rank, &p->data,
      sizeof(packet_header) + sizeof(struct mv_rdz), &p->context);
}

static void mv_sent_write_fin(mvh* mv, mv_packet* p_ctx)
{
  mv_ctx* req = (mv_ctx*)p_ctx->data.content.rdz.sreq;
  mv_set_proto(p_ctx, MV_PROTO_READY_FIN);
  mv_server_send(mv->server, req->rank, &p_ctx->data,
      sizeof(packet_header) + sizeof(struct mv_rdz), &p_ctx->context);
  mv_key key = mv_make_key(req->rank, (1 << 30) | req->tag);
  mv_value value = 0;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    req->type = REQ_DONE;
    if (req->sync) thread_signal(req->sync);
  }
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
  mv_pool_put_to(mv->pkpool, p_ctx, p_ctx->data.header.poolid);
}

const mv_proto_spec_t mv_proto[11] = {
  {0, 0}, // Reserved for doing nothing.
  {mv_recv_short, 0},
  {mv_recv_short, mv_sent_short_wait},
  {mv_recv_recv_ready, 0},
  {mv_recv_send_ready_fin, 0},
  {0, mv_sent_write_fin},
  {mv_recv_am, 0},
  {mv_recv_short_enqueue, 0},
  {mv_recv_rts, 0},
  {mv_recv_rtr, 0},
  {mv_recv_rdz_enqueue_done, mv_sent_rdz_enqueue_done},
};

