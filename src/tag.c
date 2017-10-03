#include "include/lc_priv.h"
#include <stdint.h>

#include "lc/pool.h"
#include "pmi.h"

lc_status lc_send_tag_p(lch* mv, struct lc_pkt* pkt, int rank, int tag, lc_req* ctx)
{
  lc_packet* p = (lc_packet*) pkt->_reserved_;
  void* src = pkt->buffer;
  size_t size = p->context.size;
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

lc_status lc_send_tag(lch* mv, const void* src, size_t size, int rank, int tag, lc_req* ctx)
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

lc_status lc_recv_tag(lch* mv, void* src, size_t size, int rank, int tag, lc_req* ctx)
{
  INIT_CTX(ctx);
  lc_key key = lc_make_key(ctx->rank, ctx->tag);
  lc_value value = (lc_value)ctx;
  if (!lc_hash_insert(mv->tbl, key, &value, CLIENT)) {
    lc_packet* p = (lc_packet*) value;
    if (p->context.proto & LC_PROTO_DATA) {
      memcpy(src, p->data.buffer, p->context.size);
      ctx->size = p->context.size;
      ctx->type = LC_REQ_DONE;
      lc_pool_put(mv->pkpool, p);
    } else {
      ctx->size = p->data.rts.size;
      lci_rdz_prepare(mv, src, size, ctx, p);
      lci_send(mv, &p->data, sizeof(struct packet_rtr),
          rank, tag, LC_PROTO_RTR | LC_PROTO_TAG, p);
    }
  }
  return LC_OK;
}
