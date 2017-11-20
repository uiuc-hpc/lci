#include "include/lc_priv.h"
#include <stdint.h>

#include "lc/pool.h"
#include "pmi.h"

lc_status lc_send_queue_p(lch* mv, struct lc_pkt* pkt, int rank, lc_info* desc, lc_req* ctx)
{
  lc_packet* p = (lc_packet*) pkt->_reserved_;
  void* src = pkt->buffer;
  size_t size = p->context.size;
  if (size <= (int) SHORT_MSG_SIZE) {
    lci_send(mv, src, size, rank, (desc->qtag << 8 | desc->qkey), LC_PROTO_DATA, p);
    ctx->int_type = LC_REQ_DONE;
  } else {
    INIT_CTX(ctx);
    ctx->lsync = desc->lsync;
    p->data.rts.req = (uintptr_t) ctx;
    p->data.rts.src_addr = (uintptr_t) src;
    p->data.rts.size = size;
    lci_send(mv, &p->data, sizeof(struct packet_rts),
             rank, (desc->qtag << 8 | desc->qkey), LC_PROTO_RTS, p);
  }
  return LC_OK;
}

lc_status lc_send_queue(lch* mv, const void* src, size_t size, int rank,
                        lc_info* desc, lc_req* ctx)
{
  LC_POOL_GET_OR_RETN(mv->pkpool, p);
  if (size <= (int) SHORT_MSG_SIZE) {
    lci_send(mv, src, size, rank, (desc->qtag << 8 | desc->qkey), LC_PROTO_DATA, p);
    ctx->sync = NULL;
    ctx->int_type = LC_REQ_DONE;
  } else {
    INIT_CTX(ctx);
    ctx->lsync = desc->lsync;
    ctx->rsync = desc->rsync;
    p->data.rts.req = (uintptr_t) ctx;
    p->data.rts.src_addr = (uintptr_t) src;
    p->data.rts.size = size;
    lci_send(mv, &p->data, sizeof(struct packet_rts),
             rank, (desc->qtag << 8 | desc->qkey), LC_PROTO_RTS, p);
  }
  return LC_OK;
}

lc_status lc_recv_queue(lch* mv, size_t* size, int* rank, lc_qtag* tag, lc_qtag key,
        lc_alloc_fn alloc_cb, void* alloc_ctx,  lc_sync rsync, lc_req* ctx)
{
#ifndef USE_CCQ
  lc_packet* p = (lc_packet*) dq_pop_bot(&mv->queue[key]);
#else
  lc_packet* p = (lc_packet*) lcrq_dequeue(&mv->queue[key]);
#endif
  if (!p) return LC_ERR_NOP;

  *rank = p->context.from;
  *tag = p->context.tag;
  size_t _size = p->context.size;
  if (!(p->context.proto & LC_PROTO_DATA))
    _size = p->data.rts.size;

  void* buf = alloc_ctx;
  if (alloc_cb)
      buf = alloc_cb(alloc_ctx, _size);

  ctx->sync = NULL;
  if (p->context.proto & LC_PROTO_DATA) {
    memcpy(buf, p->data.buffer, _size);
    lc_pool_put(mv->pkpool, p);
    ctx->type = LC_REQ_DONE;
    *size = _size;
  } else {
    ctx->lsync = rsync;
    int rank = p->context.from;
    lci_rdz_prepare(mv, buf, _size, ctx, p);
    lci_send(mv, &p->data, sizeof(struct packet_rtr),
             rank, 0, LC_PROTO_RTR, p);
    ctx->type = LC_REQ_PEND;
    *size = _size;
  }
  return LC_OK;
}
