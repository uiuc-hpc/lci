#include "include/lc_priv.h"
#include <stdint.h>

#include "lc/pool.h"
#include "pmi.h"

lc_status lc_send_queue_p(lch* mv, struct lc_pkt* pkt, lc_req* ctx)
{
  lc_packet* p = (lc_packet*) pkt->_reserved_;
  void* src = pkt->buffer;
  int size = p->context.size;
  int rank = pkt->rank;
  int tag = pkt->tag;
  if (size <= (int) SHORT_MSG_SIZE) {
    p->context.proto = LC_PROTO_SHORT_QUEUE;
    lci_send(mv, src, size, rank, tag, p);
    ctx->type = LC_REQ_DONE;
  } else {
    INIT_CTX(ctx);
    p->context.proto = LC_PROTO_RTS_QUEUE;
    p->data.rts.sreq = (uintptr_t) ctx;
    p->data.rts.size = size;
    lci_send(mv, &p->data, sizeof(struct packet_rts),
             rank, tag, p);
  }
  return LC_OK;
}

lc_status lc_send_queue(lch* mv, const void* src, int size, int rank, int tag, lc_req* ctx)
{
  LC_POOL_GET_OR_RETN(mv->pkpool, p);
  if (size <= (int) SHORT_MSG_SIZE) {
    p->context.proto = LC_PROTO_SHORT_QUEUE;
    lci_send(mv, src, size, rank, tag, p);
    ctx->type = LC_REQ_DONE;
  } else {
    INIT_CTX(ctx);
    p->context.proto = LC_PROTO_RTS_QUEUE;
    p->data.rts.sreq = (uintptr_t) ctx;
    p->data.rts.size = size;
    lci_send(mv, &p->data, sizeof(struct packet_rts),
             rank, tag, p);
  }
  return LC_OK;
}

lc_status lc_recv_queue_probe(lch* mv, int* size, int* rank, int *tag, lc_req* ctx)
{
#ifndef USE_CCQ
  lc_packet* p = (lc_packet*) dq_pop_bot(&mv->queue);
#else
  lc_packet* p = (lc_packet*) lcrq_dequeue(&mv->queue);
#endif
  if (p == NULL) return LC_ERR_NOP;
  *rank = p->context.from;
  if (p->context.proto != LC_PROTO_RTS_QUEUE) {
    *size = p->context.size;
  } else {
    *size = p->data.rts.size;
  }
  *tag = p->context.tag;
  ctx->packet = p;
  ctx->type = LC_REQ_PEND;
  return LC_OK;
}

lc_status lc_recv_queue(lch* mv, void* buf, lc_req* ctx)
{
  lc_packet* p = (lc_packet*) ctx->packet;
  if (p->context.proto != LC_PROTO_RTS_QUEUE) {
    memcpy(buf, p->data.buffer, p->context.size);
    lc_pool_put(mv->pkpool, p);
    ctx->type = LC_REQ_DONE;
    return LC_OK;
  } else {
    int rank = p->context.from;
    lci_rdz_prepare(mv, buf, p->data.rts.size, ctx, p);
    p->context.proto = LC_PROTO_RTR_QUEUE;
    lci_send(mv, &p->data, sizeof(struct packet_rtr),
             rank, 0, p);
    return LC_ERR_NOP;
  }
}
