#include "include/lc_priv.h"
#include <stdint.h>

#include "lc/pool.h"
#include "pmi.h"

lc_status lc_send_tag_p(lch* mv, struct lc_pkt* pkt, lc_req* ctx)
{
  lc_packet* p = (lc_packet*) pkt->_reserved_;
  void* src = pkt->buffer;
  int size = p->context.size;
  int rank = pkt->rank;
  int tag = pkt->tag;
  if (size <= (int) SHORT_MSG_SIZE) {
    lci_send(mv, src, p->context.size, rank, tag, LC_PROTO_DATA | LC_PROTO_TAG, p);
    ctx->int_type = LC_REQ_DONE;
  } else {
    INIT_CTX(ctx);
    p->data.rts.req = (uintptr_t) ctx;
    p->data.rts.src_addr = (uintptr_t) src;
    p->data.rts.size = size;
    lci_send(mv, &p->data, sizeof(struct packet_rts),
             rank, tag, LC_PROTO_RTS | LC_PROTO_TAG, p);
  }
  return LC_OK;
}

lc_status lc_send_tag(lch* mv, const void* src, int size, int rank, int tag, lc_req* ctx)
{
  LC_POOL_GET_OR_RETN(mv->pkpool, p);
  if (size <= (int) SHORT_MSG_SIZE) {
    lci_send(mv, src, size, rank, tag, LC_PROTO_DATA | LC_PROTO_TAG, p);
    ctx->int_type = LC_REQ_DONE;
  } else {
    INIT_CTX(ctx);
    p->data.rts.req = (uintptr_t) ctx;
    p->data.rts.src_addr = (uintptr_t) src;
    p->data.rts.size = size;
    lci_send(mv, &p->data, sizeof(struct packet_rts),
             rank, tag, LC_PROTO_RTS | LC_PROTO_TAG, p);
  }
  return LC_OK;
}

static void lc_recv_tag_final(lch* mv, lc_req* req, lc_packet* p)
{
  if (p->context.proto & LC_PROTO_DATA) {
    memcpy(req->buffer, p->data.buffer, req->size);
    LC_SET_REQ_DONE_AND_SIGNAL(req);
    lc_pool_put(mv->pkpool, p);
  } else {
    lci_rdz_prepare(mv, req->buffer, req->size, req, p);
    lci_send(mv, &p->data, sizeof(struct packet_rtr),
        req->rank, req->tag, LC_PROTO_RTR | LC_PROTO_TAG, p);
  }
}

lc_status lc_recv_tag(lch* mv, void* src, int size, int rank, int tag, lc_req* ctx)
{
  INIT_CTX(ctx);
  lc_key key = lc_make_key(ctx->rank, ctx->tag);
  lc_value value = (lc_value)ctx;
  ctx->finalize = lc_recv_tag_final;
  if (!lc_hash_insert(mv->tbl, key, &value, CLIENT)) {
    lc_packet* p = (lc_packet*) value;
    if (p->context.proto & LC_PROTO_DATA) {
      memcpy(src, p->data.buffer, size);
      ctx->type = LC_REQ_DONE;
      lc_pool_put(mv->pkpool, p);
    } else {
      lci_rdz_prepare(mv, src, size, ctx, p);
      lci_send(mv, &p->data, sizeof(struct packet_rtr),
          rank, tag, LC_PROTO_RTR | LC_PROTO_TAG, p);
    }
  }
  return LC_OK;
}
