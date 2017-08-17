#include "include/lc_priv.h"
#include <stdint.h>

#include "lc/pool.h"
#include "pmi.h"

static lc_status lc_send_tag_post(lch* mv __UNUSED__, lc_req* ctx, lc_sync* sync)
{
  if (!ctx) return 1;
  if (ctx->size <= (int) SHORT_MSG_SIZE || ctx->type == REQ_DONE) {
    return LC_OK;
  } else {
    ctx->sync = sync;
    return LC_ERR_NOP;
  }
}

lc_status lc_send_tag(lch* mv, const void* src, int size, int rank, int tag, lc_req* ctx)
{
  if (size <= (int) SHORT_MSG_SIZE) {
    LC_POOL_GET_OR_RETN(mv->pkpool, p);
    p->context.proto = LC_PROTO_SHORT_MATCH;
    lci_send(mv, src, size, rank, tag, p);
    ctx->type = REQ_DONE;
  } else {
    INIT_CTX(ctx);
    lc_key key = lc_make_rdz_key(rank, tag);
    lc_value value = (lc_value)ctx;
    if (!lc_hash_insert(mv->tbl, key, &value, CLIENT)) {
      lc_packet* p = (lc_packet*) value;
      p->context.req = (uintptr_t)ctx;
      p->context.proto = LC_PROTO_LONG_MATCH;
      lci_put(mv, ctx->buffer, ctx->size, p->context.from,
          p->data.rtr.tgt_addr, p->data.rtr.rkey,
          0, p->data.rtr.comm_id, p);
    }
  }
  ctx->post = lc_send_tag_post;
  return LC_OK;
}

static lc_status lc_recv_tag_post(lch* mv, lc_req* ctx, lc_sync* sync)
{
  ctx->sync = sync;
  lc_key key = lc_make_key(ctx->rank, ctx->tag);
  lc_value value = (lc_value)ctx;
  if (!lc_hash_insert(mv->tbl, key, &value, CLIENT)) {
    ctx->type = REQ_DONE;
    lc_packet* p_ctx = (lc_packet*)value;
    if (ctx->size <= (int) SHORT_MSG_SIZE)
      memcpy(ctx->buffer, p_ctx->data.buffer, ctx->size);
    lc_pool_put(mv->pkpool, p_ctx);
    return LC_OK;
  }
  return LC_ERR_NOP;
}

lc_status lc_recv_tag(lch* mv, void* src, int size, int rank, int tag, lc_req* ctx)
{
  if (size <= (int) SHORT_MSG_SIZE) {
    INIT_CTX(ctx);
  } else {
    LC_POOL_GET_OR_RETN(mv->pkpool, p);
    INIT_CTX(ctx);
    lci_rdz_prepare(mv, src, size, ctx, p);
    p->context.proto = LC_PROTO_RTR_MATCH;
    lci_send(mv, &p->data, sizeof(struct packet_rtr),
             rank, tag, p);
  }
  ctx->post = lc_recv_tag_post;
  return LC_OK;
}
