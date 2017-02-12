#include "mv_priv.h"

static void mv_recv_am(mvh* mv, mv_packet* p)
{
  uint8_t fid = (uint8_t)p->data.header.tag;
  uint32_t* buffer = (uint32_t*)p->data.content.buffer;
  uint32_t size = buffer[0];
  char* data = (char*)&buffer[1];
  ((_0_arg)mv->am_table[fid])(data, size);
}

static void mv_recv_short_match(mvh* mv, mv_packet* p)
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
#ifndef USE_CCQ
  dq_push_top(&mv->queue, (void*) p);
#else
  lcrq_enqueue(&mv->queue, (void*) p);
#endif
}

static void mv_sent_rdz_enqueue_done(mvh* mv, mv_packet* p)
{
  mv_ctx* ctx = (mv_ctx*) p->data.content.rdz.sreq;
  ctx->type = REQ_DONE;
  mv_pool_put(mv->pkpool, p);
}

static void mv_recv_rtr_match(mvh* mv, mv_packet* p)
{
  mv_key key = mv_make_rdz_key(p->data.header.from, p->data.header.tag);
  mv_value value = (mv_value)p;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    proto_complete_rndz(mv, p, (mv_ctx*)value);
  }
}

static void mv_recv_rtr_queue(mvh* mv, mv_packet* p)
{
  mv_ctx* ctx = (mv_ctx*) p->data.content.rdz.sreq;
  int rank = p->data.header.from;
  p->data.header.proto = MV_PROTO_LONG_ENQUEUE;
  mv_server_rma_signal(mv->server, rank, ctx->buffer,
      (void*) p->data.content.rdz.tgt_addr,
      mv_server_heap_rkey(mv->server, rank), ctx->size,
      (1 << 31) | (p->data.content.rdz.comm_id), &p->context);
}

static void mv_recv_rts_queue(mvh* mv, mv_packet* p)
{
  void* ptr = mv_alloc(p->data.header.size);
  int rank = p->data.header.from;
  uint32_t comm_idx = p->context.pid;
  mv_comm_id[comm_idx] = (uintptr_t) p;
  p->data.header.from = mv->me;
  p->data.header.to = rank;
  p->data.header.proto = MV_PROTO_RTR_ENQUEUE;
  p->data.content.rdz.tgt_addr = (uintptr_t) ptr;
  p->data.content.rdz.comm_id = (uint32_t) comm_idx;
  mv_server_send(mv->server, rank, &p->data,
      sizeof(struct packet_header) + sizeof(struct mv_rdz), &p->context);
}

static void mv_sent_rdz_match_done(mvh* mv, mv_packet* p)
{
  mv_ctx* req = (mv_ctx*) p->data.content.rdz.sreq;
  mv_key key = mv_make_key(req->rank, (1 << 30) | req->tag);
  mv_value value = 0;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    req->type = REQ_DONE;
    if (req->sync) thread_signal(req->sync);
  }
  mv_pool_put(mv->pkpool, p);
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
  mv_pool_put_to(mv->pkpool, p_ctx, p_ctx->context.poolid);
}

const mv_proto_spec_t mv_proto[10] = {
  {0, 0}, // Reserved for doing nothing.
  {mv_recv_short_match, MV_PROTO_DONE},
  {mv_recv_short_match, mv_sent_short_wait},
  {mv_recv_rtr_match, 0},
  {0, mv_sent_rdz_match_done},
  {mv_recv_am, MV_PROTO_DONE},
  {mv_recv_short_enqueue, MV_PROTO_DONE},
  {mv_recv_rts_queue, MV_PROTO_DONE},
  {mv_recv_rtr_queue, 0},
  {0, mv_sent_rdz_enqueue_done},
};

