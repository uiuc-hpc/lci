#include "include/lc_priv.h"
#include <stdint.h>

#include "lc/pool.h"
#include "pmi.h"

static lc_status lc_send_tag_post(lch* mv __UNUSED__, lc_req* ctx, lc_sync* sync)
{
  if (!ctx) return 1;
  if (ctx->size <= (int) SHORT_MSG_SIZE || ctx->type == LC_REQ_DONE) {
    return LC_OK;
  } else {
    ctx->sync = sync;
    return LC_ERR_NOP;
  }
}

lc_status lc_send_tag_p(lch* mv, struct lc_pkt* pkt, lc_req* ctx)
{
  lc_packet* p = (lc_packet*) pkt->_reserved_;
  void* src = pkt->buffer;
  int size = p->context.size;
  int rank = pkt->rank;
  int tag = pkt->tag;
  if (size <= (int) SHORT_MSG_SIZE) {
    p->context.proto = LC_PROTO_SHORT_TAG;
    lci_send(mv, src, p->context.size, rank, tag, p);
    ctx->type = LC_REQ_DONE;
  } else {
    INIT_CTX(ctx);
    p->context.proto = LC_PROTO_RTS_TAG;
    p->data.rts.sreq = (uintptr_t) ctx;
    p->data.rts.size = size;
    lci_send(mv, &p->data, sizeof(struct packet_rts),
             rank, tag, p);
  }
  ctx->post = lc_send_tag_post;
  return LC_OK;
}

lc_status lc_send_tag(lch* mv, const void* src, int size, int rank, int tag, lc_req* ctx)
{
  LC_POOL_GET_OR_RETN(mv->pkpool, p);
  if (size <= (int) SHORT_MSG_SIZE) {
    p->context.proto = LC_PROTO_SHORT_TAG;
    lci_send(mv, src, size, rank, tag, p);
    ctx->type = LC_REQ_DONE;
  } else {
    INIT_CTX(ctx);
    p->context.proto = LC_PROTO_RTS_TAG;
    p->data.rts.sreq = (uintptr_t) ctx;
    p->data.rts.size = size;
    lci_send(mv, &p->data, sizeof(struct packet_rts),
             rank, tag, p);
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
    lc_packet* p_ctx = (lc_packet*)value;
    if (ctx->size <= (int) SHORT_MSG_SIZE) {
      ctx->type = LC_REQ_DONE;
      memcpy(ctx->buffer, p_ctx->data.buffer, ctx->size);
      lc_pool_put(mv->pkpool, p_ctx);
      return LC_OK;
    } else {
      p_ctx->context.proto = LC_PROTO_RTR_TAG;
      lci_rdz_prepare(mv, ctx->buffer, ctx->size, ctx, p_ctx);
      lci_send(mv, &p_ctx->data, sizeof(struct packet_rtr),
          ctx->rank, ctx->tag, p_ctx);
    }
  }
  return LC_ERR_NOP;
}

lc_status lc_recv_tag(lch* mv, void* src, int size, int rank, int tag, lc_req* ctx)
{
  INIT_CTX(ctx);
  ctx->post = lc_recv_tag_post;
  return LC_OK;
}
