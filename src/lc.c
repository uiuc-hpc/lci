#include "include/lc_priv.h"
#include <stdint.h>

#include "lc/pool.h"
#include "pmi.h"

size_t server_max_inline;
__thread int lc_core_id = -1;

void* lc_heap_ptr(lch* mv)
{
  return lc_server_heap_ptr(mv->server);
}

void lc_open(lch** ret)
{
  struct lc_struct* mv = malloc(sizeof(struct lc_struct));
  mv->ncores = sysconf(_SC_NPROCESSORS_ONLN) / THREAD_PER_CORE;

  lc_hash_create(&mv->tbl);
  // Init queue protocol.
#ifndef USE_CCQ
  dq_init(&mv->queue);
#else
  lcrq_init(&mv->queue);
#endif

  // Prepare the list of packet.
  uint32_t npacket = MAX(MAX_PACKET, mv->size * 4);
  lc_server_init(mv, npacket * LC_PACKET_SIZE * 2, &mv->server);

  uintptr_t base_packet = (uintptr_t) lc_heap_ptr(mv) + 4096; // the commid should not be 0.
  lc_pool_create(&mv->pkpool);

  for (unsigned i = 0; i < npacket; i++) {
    lc_packet* p = (lc_packet*) (base_packet + i * LC_PACKET_SIZE);
    p->context.poolid = 0; // default to the first pool -- usually server.
    lc_pool_put(mv->pkpool, p);
  }

  // Prepare the list of rma_ctx.
  uintptr_t base_rma = (base_packet + npacket * LC_PACKET_SIZE);
  lc_pool_create(&mv->rma_pool);
  for(unsigned i = 0; i < MAX_PACKET; i++) {
    struct lc_rma_ctx* rma_ctx = (struct lc_rma_ctx*) (base_rma + i * sizeof(struct lc_rma_ctx));
    lc_pool_put(mv->rma_pool, rma_ctx);
  }

  PMI_Barrier();

  *ret = mv;
}

void lc_close(lch* mv)
{
  PMI_Barrier();
  lc_server_finalize(mv->server);
  lc_hash_destroy(mv->tbl);
  lc_pool_destroy(mv->pkpool);
#ifdef USE_CCQ
  lcrq_destroy(&mv->queue);
#endif
  free(mv);
}

void lc_send_pkt(lch* mv, struct lc_pkt* pkt, int rank, int tag, lc_req* ctx)
{
  lc_packet* p = (lc_packet*) pkt->_reserved_;
  p->context.req = (uintptr_t) ctx;
  p->context.proto = LC_PROTO_PERSIS;
  lci_send(mv, &p->data, p->context.size,
           rank, tag, p);
}

int lc_send_put(lch* mv, void* src, int size, int rank, lc_addr* dst, lc_req* ctx)
{
  lc_packet* p = (lc_packet*) lc_pool_get_nb(mv->pkpool);
  if (!p) { ctx->type = LC_REQ_NULL; return 0; };
  struct lc_rma_ctx* dctx = (struct lc_rma_ctx*) dst;
  ctx->type = LC_REQ_PENDING;
  p->context.req = (uintptr_t) ctx;
  lc_server_rma(mv->server, rank, src, dctx->addr, dctx->rkey, size, p, LC_PROTO_LONG_PUT);
  return 1;
}

int lc_send_put_signal(lch* mv, void* src, int size, int rank, lc_addr* dst, lc_req* ctx)
{
  lc_packet* p = (lc_packet*) lc_pool_get_nb(mv->pkpool);
  if (!p) { ctx->type = LC_REQ_NULL; return 0; };
  struct lc_rma_ctx* dctx = (struct lc_rma_ctx*) dst;
  ctx->type = LC_REQ_PENDING;
  p->context.req = (uintptr_t) ctx;
  p->context.proto = LC_PROTO_LONG_PUT;
  lci_put(mv, src, size, rank, dctx->addr, dctx->rkey,
          RMA_SIGNAL_SIMPLE, dctx->sid, p);
  return 1;
}

int lc_rma_create(lch* mv, void* buf, size_t size, lc_addr** rctx_ptr)
{
  struct lc_rma_ctx* rctx = (struct lc_rma_ctx*) lc_pool_get(mv->rma_pool);
  uintptr_t rma = lc_server_rma_reg(mv->server, buf, size);
  rctx->addr = (uintptr_t) buf;
  rctx->rkey = lc_server_rma_key(rma);
  rctx->sid = (uint32_t) ((uintptr_t) rctx - (uintptr_t) lc_heap_ptr(mv));
  *rctx_ptr = rctx;
  return 1;
}

int lc_recv_put_signal(lch* mv __UNUSED__, lc_addr* rctx, lc_req* ctx)
{
  rctx->req = (uintptr_t) ctx;
  ctx->type = LC_REQ_PENDING;
  return 1;
}

int lc_progress(lch* mv)
{
  return lc_server_progress(mv->server);
}

lc_status lc_pkt_init(lch* mv, int size, int rank, int tag, struct lc_pkt* pkt)
{
  LC_POOL_GET_OR_RETN(mv->pkpool, p);
  p->context.size = size;
  pkt->rank = rank;
  pkt->tag = tag;
  p->context.runtime = 0;
  pkt->_reserved_ = p;
  if (size < (int) SHORT_MSG_SIZE) {
    pkt->buffer = &p->data;
  } else {
    pkt->buffer = memalign(4096, size);
    p->data.rts.size = size;
  }
  return LC_OK;
}

void lc_pkt_fini(lch* mv, struct lc_pkt* pkt)
{
  if (pkt->_reserved_) {
    lc_packet* p = (lc_packet*) pkt->_reserved_;
    if (pkt->buffer != &(p->data))
      free(pkt->buffer);
    lc_pool_put(mv->pkpool, p);
  }
}

int lc_id(lch* mv) {
  return mv->me;
}

int lc_size(lch* mv) {
  return mv->size;
}
