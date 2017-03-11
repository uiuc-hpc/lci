#include "mv_priv.h"

#if 0
static void mv_recv_am(mvh* mv, mv_packet* p)
{
  uint8_t fid = (uint8_t)p->data.header.tag;
  uint32_t* buffer = (uint32_t*)p->data.buffer;
  uint32_t size = buffer[0];
  char* data = (char*)&buffer[1];
  ((_0_arg)mv->am_table[fid])(data, size);
}
#endif

static void mv_recv_short_match(mvh* mv, mv_packet* p)
{
  const mv_key key = mv_make_key(p->context.from, p->context.tag);
  mv_value value = (mv_value)p;

  if (!mv_hash_insert(mv->tbl, key, &value)) {
    // comm-thread comes later.
    mv_ctx* req = (mv_ctx*)value;
    memcpy(req->buffer, p->data.buffer, req->size);
    req->type = REQ_DONE;
    if (req->sync) thread_signal(req->sync);
    mv_pool_put(mv->pkpool, p);
  }
}

static void mv_sent_rdz_enqueue_done(mvh* mv, mv_packet* p)
{
  mv_ctx* ctx = (mv_ctx*) p->context.req;
  ctx->type = REQ_DONE;
  mv_pool_put(mv->pkpool, p);
}

static void mv_recv_rtr_match(mvh* mv, mv_packet* p)
{
  mv_key key = mv_make_rdz_key(p->context.from, p->context.tag);
  mv_value value = (mv_value)p;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    proto_complete_rndz(mv, p, (mv_ctx*)value);
  }
}

static void mv_recv_rtr_queue(mvh* mv, mv_packet* p)
{
  int rank = p->context.from;
  mv_ctx* ctx = p->context.req = (uintptr_t) p->data.rtr.sreq;
  mv_server_rma_signal(mv->server, rank, (void*) ctx->buffer,
      p->data.rtr.tgt_addr,
      p->data.rtr.rkey,
      ctx->size,
      RMA_SIGNAL_QUEUE | (p->data.rtr.comm_id), p, MV_PROTO_LONG_QUEUE);
}

static void mv_sent_rdz_match_done(mvh* mv, mv_packet* p)
{
  mv_ctx* req = (mv_ctx*) p->context.req;
  mv_key key = mv_make_key(req->rank, RDZ_MATCH_TAG | req->tag);
  mv_value value = 0;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    req->type = REQ_DONE;
    if (req->sync) thread_signal(req->sync);
  }
  mv_pool_put(mv->pkpool, p);
}

static void mv_sent_short(mvh* mv, mv_packet* p_ctx)
{
  mv_ctx* ctx = (mv_ctx*) p_ctx->context.req;
  ctx->type = REQ_DONE;
  if (ctx->sync) thread_signal(ctx->sync);
  if (p_ctx->context.poolid)
    mv_pool_put_to(mv->pkpool, p_ctx, p_ctx->context.poolid);
  else
    mv_pool_put(mv->pkpool, p_ctx);
}

static void mv_sent_put(mvh* mv, mv_packet* p_ctx)
{
  mv_ctx* req = (mv_ctx*) p_ctx->context.req;
  req->type = REQ_DONE;
  mv_pool_put(mv->pkpool, p_ctx);
}

static void mv_sent_persis(mvh* mv, mv_packet* p_ctx)
{
  mv_ctx* req = (mv_ctx*) p_ctx->context.req;
  req->type = REQ_DONE;
}

static void mv_sent_done(mvh* mv, mv_packet* p_ctx)
{
  if (p_ctx->context.poolid)
    mv_pool_put_to(mv->pkpool, p_ctx, p_ctx->context.poolid);
  else
    mv_pool_put(mv->pkpool, p_ctx);
}

static void mv_recv_queue_packet_short(mvh* mv, mv_packet* p)
{
  p->context.proto = MV_PROTO_SHORT_QUEUE;
#ifndef USE_CCQ
  dq_push_top(&mv->queue, (void*) p);
#else
  lcrq_enqueue(&mv->queue, (void*) p);
#endif
}

static void mv_recv_queue_packet_long(mvh* mv, mv_packet* p)
{
  p->context.proto = MV_PROTO_RTS_QUEUE;
#ifndef USE_CCQ
  dq_push_top(&mv->queue, (void*) p);
#else
  lcrq_enqueue(&mv->queue, (void*) p);
#endif
}

const mv_proto_spec_t mv_proto[10] = {
  {0, 0}, // Reserved for doing nothing.

  /** Tag Matching protocol */
  {mv_recv_short_match, mv_sent_done},
  {mv_recv_rtr_match, 0},
  {0, mv_sent_rdz_match_done},

  /** Queue Matching protocol */
  {mv_recv_queue_packet_short, mv_sent_done},
  {mv_recv_queue_packet_long, mv_sent_done},
  {mv_recv_rtr_queue, 0},
  {0, mv_sent_rdz_enqueue_done},

  /** Other experimental */
  {0, mv_sent_put},
  {mv_recv_queue_packet_short, mv_sent_persis}, // PERSIS
};


//FIXME(danghvu): Experimental stuff to work-around memory registration.
#ifdef SERVER_IBV_H_
uintptr_t get_dma_mem(void* server, void* buf, size_t s)
{
  return _real_ibv_reg((mv_server*) server, buf, s);
}

int free_dma_mem(uintptr_t mem)
{
  return ibv_dereg_mr((struct ibv_mr*) mem);
}
#else
#ifdef SERVER_OFI_H_
uintptr_t get_dma_mem(void* server, void* buf, size_t s)
{
  return _real_ofi_reg((mv_server*) server, buf, s);
}

int free_dma_mem(uintptr_t mem)
{
  return fi_close((struct fid*) mem);
}
#else
#ifdef SERVER_PSM_H_
uintptr_t get_dma_mem(void* server, void* buf, size_t s) {
  return _real_psm_reg((mv_server*) server, buf, s);
}
int free_dma_mem(uintptr_t mem) {
  return _real_psm_free(mem);
}
#endif
#endif
#endif
