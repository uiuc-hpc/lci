#include "include/lc_priv.h"
#include <stdint.h>

lc_status lc_send_put(lch* mv, void* src, size_t size, lc_addr* dst, lc_info* info, lc_req* ctx)
{
  LC_POOL_GET_OR_RETN(mv->pkpool, p);
  struct lc_rma_ctx* dctx = (struct lc_rma_ctx*) dst;
  ctx->type = LC_REQ_PEND;
  ctx->lsync = info->lsync;
  ctx->rsync = info->rsync;
  ctx->sync = NULL;
  p->context.req = ctx;
  p->context.proto = LC_PROTO_LONG;
  lci_put(mv, src, size, dctx->rank, dctx->addr, info->offset, dctx->rkey,
          dctx->sid, p);
  return LC_OK;
}

lc_status lc_send_get(lch* mv, void* src, size_t size, lc_addr* dst, lc_info* info, lc_req* ctx)
{
  LC_POOL_GET_OR_RETN(mv->pkpool, p);
  struct lc_rma_ctx* dctx = (struct lc_rma_ctx*) dst;
  ctx->type = LC_REQ_PEND;
  ctx->lsync = info->lsync;
  ctx->rsync = info->rsync;
  ctx->sync = NULL;
  p->context.req = ctx;
  p->context.proto = LC_PROTO_LONG;
  lci_get(mv, src, size, dctx->rank, dctx->addr, info->offset, dctx->rkey,
          dctx->sid, p);
  return LC_OK;
}

lc_status lc_recv_put(lch* mv, lc_addr* rctx, lc_req* req)
{
  lc_packet* p = (lc_packet*) ((uintptr_t)lc_heap_ptr(mv) + (rctx->sid >> 2));
  req->type = LC_REQ_PEND;
  req->sync = NULL;
  p->context.req = req;
  return LC_OK;
}

lc_status lc_rma_init(lch* mv, void* buf, size_t size, lc_addr* rctx)
{
  uintptr_t rma = lc_server_rma_reg(mv->server, buf, size);
  rctx->addr = (uintptr_t) buf;
  rctx->rkey = lc_server_rma_key(rma);
  rctx->rank = lc_id(mv);
  LC_POOL_GET_OR_RETN(mv->pkpool, p);
  p->context.req = 0;
  p->context.rma_mem = rma;
  p->context.proto = LC_PROTO_DATA;
  rctx->sid = MAKE_SIG(LC_PROTO_TGT, (uint32_t)((uintptr_t)p - (uintptr_t)lc_heap_ptr(mv)));
  return LC_OK;
}

void lc_rma_fini(lch*mv, lc_addr* rctx)
{
  lc_packet* p = (lc_packet*) ((uintptr_t)lc_heap_ptr(mv) + (rctx->sid >> 2));
  lc_server_rma_dereg(p->context.rma_mem);
  lc_pool_put(mv->pkpool, p);
}
