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

void lc_open(lch** ret, int num_qs)
{
  struct lc_struct* mv = 0;
  posix_memalign((void**) &mv, 64, sizeof(struct lc_struct));
  mv->ncores = sysconf(_SC_NPROCESSORS_ONLN) / THREAD_PER_CORE;

  lc_hash_create(&mv->tbl);
  posix_memalign((void**) &mv->queue, 64, sizeof(*(mv->queue)) * num_qs);

  for (int i = 0; i < num_qs; i++) {
    // Init queue protocol.
#ifndef USE_CCQ
    dq_init(&mv->queue[i]);
#else
    lcrq_init(&mv->queue[i]);
#endif
  }

  // Prepare the list of packet.
  lc_server_init(mv, MAX_PACKET * LC_PACKET_SIZE * 2, &mv->server);

  uintptr_t base_packet = (uintptr_t) lc_heap_ptr(mv) + 4096; // the commid should not be 0.
  lc_pool_create(&mv->pkpool);

  for (unsigned i = 0; i < MAX_PACKET; i++) {
    lc_packet* p = (lc_packet*) (base_packet + i * LC_PACKET_SIZE);
    p->context.poolid = 0;
    lc_pool_put(mv->pkpool, p);
  }

  *ret = mv;
  PMI_Barrier();
  while (mv->server->recv_posted == 0)
    lc_progress(mv);
}

void lc_close(lch* mv)
{
  lc_server_finalize(mv->server);
  lc_hash_destroy(mv->tbl);
  lc_pool_destroy(mv->pkpool);
#ifdef USE_CCQ
  lcrq_destroy(&mv->queue);
#endif
  free(mv);
}

lc_status lc_send_put(lch* mv, void* src, size_t size, lc_addr* dst, lc_req* ctx)
{
  LC_POOL_GET_OR_RETN(mv->pkpool, p);
  struct lc_rma_ctx* dctx = (struct lc_rma_ctx*) dst;
  ctx->type = LC_REQ_PEND;
  ctx->sync = NULL;
  p->context.req = ctx;
  p->context.proto = LC_PROTO_LONG;
  lci_put(mv, src, size, dctx->rank, dctx->addr, dctx->rkey,
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

int lc_progress(lch* mv)
{
  return lc_server_progress(mv->server);
}

lc_status lc_pkt_init(lch* mv, size_t size, struct lc_pkt* pkt)
{
  LC_POOL_GET_OR_RETN(mv->pkpool, p);
  p->context.size = size;
  p->context.runtime = 0;
  pkt->_reserved_ = p;
  if (size < (int) SHORT_MSG_SIZE) {
    pkt->buffer = &p->data;
  } else {
    posix_memalign((void**) &pkt->buffer, 4096, size);
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

uintptr_t get_dma_mem(void* server, void* buf, size_t s)
{
  return _real_server_reg((lc_server*) server, buf, s);
}

int free_dma_mem(uintptr_t mem)
{
  _real_server_dereg(mem);
  return 1;
}
