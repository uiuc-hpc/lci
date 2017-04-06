#include <mpi.h>
#include "include/lc_priv.h"
#include <stdint.h>
#include "pool.h"

#include "dreg/dreg.h"

size_t server_max_inline;
__thread int lc_core_id = -1;

void* lc_heap_ptr(lch* mv)
{
  return lc_server_heap_ptr(mv->server);
}

void lc_open(size_t heap_size, lch** ret)
{
  struct lc_struct* mv = 0;
  posix_memalign((void**) &mv, 4096, sizeof(struct lc_struct));

  setenv("I_MPI_FABRICS", "ofa", 1);
  setenv("MPICH_ASYNC_PROGRESS", "0", 1);
  setenv("MV2_ASYNC_PROGRESS", "0", 1);
  setenv("MV2_ENABLE_AFFINITY", "0", 1);
  setenv("MV2_USE_LAZY_MEM_UNREGISTER", "0", 1);

  lc_pool_init();
  lc_hash_create(&mv->tbl);

  lc_server_init(mv, heap_size, &mv->server);

  // Init queue protocol.
#ifndef USE_CCQ
  dq_init(&mv->queue);
#else
  lcrq_init(&mv->queue);
#endif

  // Prepare the list of packet.
  uint32_t npacket = MAX(MAX_PACKET, mv->size * 4);
  uintptr_t sbuf_addr = (uintptr_t) lc_heap_ptr(mv);
  uintptr_t base_packet = (uintptr_t) sbuf_addr + 4096;
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

  MPI_Barrier(MPI_COMM_WORLD);
  *ret = mv;
}

void lc_close(lch* mv)
{
  MPI_Barrier(MPI_COMM_WORLD);
  lc_server_finalize(mv->server);
  lc_hash_destroy(mv->tbl);
  lc_pool_destroy(mv->pkpool);
#ifdef USE_CCQ
  lcrq_destroy(&mv->queue);
#endif
  free(mv);
  MPI_Finalize();
}

int lc_send(lch* mv, const void* src, int size, int rank, int tag, lc_ctx* ctx)
{
  if (size <= (int) SHORT_MSG_SIZE) {
    lc_packet* p = lc_pool_get_nb(mv->pkpool);
    if (!p) return 0;
    p->context.proto = LC_PROTO_SHORT_MATCH;
    lci_send(mv, src, size, rank, tag, p);
    ctx->type = REQ_DONE;
  } else {
    INIT_CTX(ctx);
    lc_key key = lc_make_rdz_key(rank, tag);
    lc_value value = (lc_value)ctx;
    if (!lc_hash_insert(mv->tbl, key, &value)) {
      lc_packet* p = (lc_packet*) value;
      p->context.req = (uintptr_t)ctx;
      p->context.proto = LC_PROTO_LONG_MATCH;
      lci_put(mv, ctx->buffer, ctx->size, p->context.from,
          p->data.rtr.tgt_addr, p->data.rtr.rkey,
          0, p->data.rtr.comm_id, p);
    }
  }
  return 1;
}

int lc_send_post(lch* mv, lc_ctx* ctx, lc_sync* sync)
{
  if (!ctx) return 1;
  if (ctx->size <= (int) SHORT_MSG_SIZE) {
    return 1;
  } else {
    if (ctx->type == REQ_DONE) return 1;
    else {
      int ret = 0;
      lc_spin_lock(&ctx->lock);
      if (ctx->type != REQ_DONE) {
        ctx->sync = sync;
      } else {
        ret = 1;
      }
      lc_spin_unlock(&ctx->lock);
      return ret;
    }
  }
}

int lc_recv(lch* mv, void* src, int size, int rank, int tag, lc_ctx* ctx)
{
  if (size <= (int) SHORT_MSG_SIZE) {
    INIT_CTX(ctx);
    return 1;
  } else {
    lc_packet* p = lc_pool_get_nb(mv->pkpool);
    if (!p) return 0;
    INIT_CTX(ctx);
    lci_rdz_prepare(mv, src, size, ctx, p);
    p->context.proto = LC_PROTO_RTR_MATCH;
    lci_send(mv, &p->data, sizeof(struct packet_rtr),
             rank, tag, p);
    return 1;
  }
}

int lc_recv_post(lch* mv, lc_ctx* ctx, lc_sync* sync)
{
  ctx->sync = sync;
  lc_key key = lc_make_key(ctx->rank, ctx->tag);
  lc_value value = (lc_value)ctx;
  if (!lc_hash_insert(mv->tbl, key, &value)) {
    ctx->type = REQ_DONE;
    if (ctx->size <= (int) SHORT_MSG_SIZE) {
      lc_packet* p_ctx = (lc_packet*)value;
      memcpy(ctx->buffer, p_ctx->data.buffer, ctx->size);
      lc_pool_put(mv->pkpool, p_ctx);
    }
    return 1;
  }
  return 0;
}

void lc_send_persis(lch* mv, lc_packet* p, int rank, int tag, lc_ctx* ctx)
{
  p->context.req = (uintptr_t) ctx;
  p->context.proto = LC_PROTO_PERSIS;
  lci_send(mv, &p->data, p->context.size,
           rank, tag, p);
}

int lc_send_queue(lch* mv, const void* src, int size, int rank, int tag, lc_ctx* ctx)
{
  lc_packet* p = (lc_packet*) lc_pool_get_nb(mv->pkpool);
  if (!p) { ctx->type = REQ_NULL; return 0; };
  if (size <= (int) SHORT_MSG_SIZE) {
    p->context.proto = LC_PROTO_SHORT_QUEUE;
    lci_send(mv, src, size, rank, tag, p);
    ctx->type = REQ_DONE;
  } else {
    INIT_CTX(ctx);
    p->context.proto = LC_PROTO_RTS_QUEUE;
    p->data.rts.sreq = (uintptr_t) ctx;
    p->data.rts.size = size;
    lci_send(mv, &p->data, sizeof(struct packet_rts),
             rank, tag, p);
  }
  return 1;
}

int lc_recv_queue(lch* mv, int* size, int* rank, int *tag, lc_ctx* ctx)
{
#ifndef USE_CCQ
  lc_packet* p = (lc_packet*) dq_pop_bot(&mv->queue);
#else
  lc_packet* p = (lc_packet*) lcrq_dequeue(&mv->queue);
#endif
  if (p == NULL) return 0;

  *rank = p->context.from;
  if (p->context.proto != LC_PROTO_RTS_QUEUE) {
    *size = p->context.size;
  } else {
    *size = p->data.rts.size;
  }
  *tag = p->context.tag;
  ctx->packet = p;
  ctx->type = REQ_PENDING;

  return 1;
}

int lc_recv_queue_post(lch* mv, void* buf, lc_ctx* ctx)
{
  lc_packet* p = (lc_packet*) ctx->packet;
  if (p->context.proto != LC_PROTO_RTS_QUEUE) {
    memcpy(buf, p->data.buffer, p->context.size);
    lc_pool_put(mv->pkpool, p);
    ctx->type = REQ_DONE;
    return 1;
  } else {
    int rank = p->context.from;
    lci_rdz_prepare(mv, buf, p->data.rts.size, ctx, p);
    p->context.proto = LC_PROTO_RTR_QUEUE;
    lci_send(mv, &p->data, sizeof(struct packet_rtr),
             rank, 0, p);
    return 0;
  }
}

int lc_send_put(lch* mv, void* src, int size, int rank, lc_addr* dst, lc_ctx* ctx)
{
  lc_packet* p = (lc_packet*) lc_pool_get_nb(mv->pkpool);
  if (!p) { ctx->type = REQ_NULL; return 0; };
  struct lc_rma_ctx* dctx = (struct lc_rma_ctx*) dst;
  ctx->type = REQ_PENDING;
  p->context.req = (uintptr_t) ctx;
  lc_server_rma(mv->server, rank, src, dctx->addr, dctx->rkey, size, p, LC_PROTO_LONG_PUT);
  return 1;
}

int lc_send_put_signal(lch* mv, void* src, int size, int rank, lc_addr* dst, lc_ctx* ctx)
{
  lc_packet* p = (lc_packet*) lc_pool_get_nb(mv->pkpool);
  if (!p) { ctx->type = REQ_NULL; return 0; };
  struct lc_rma_ctx* dctx = (struct lc_rma_ctx*) dst;
  ctx->type = REQ_PENDING;
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

int lc_recv_put_signal(lch* mv, lc_addr* rctx, lc_ctx* ctx)
{
  rctx->req = (uintptr_t) ctx;
  ctx->type = REQ_PENDING;
  return 1;
}

void lc_progress(lch* mv)
{
  lc_server_progress(mv->server);
}

size_t lc_get_ncores()
{
  return sysconf(_SC_NPROCESSORS_ONLN) / THREAD_PER_CORE;
}

lc_packet* lc_alloc_packet(lch* mv, int size)
{
  lc_packet* p = lc_pool_get_nb(mv->pkpool);
  if (!p) return NULL;
  if (size >= (int) SHORT_MSG_SIZE) {
    fprintf(stderr, "Message size %d too big, try < %d\n", size, (int) SHORT_MSG_SIZE);
    exit(EXIT_FAILURE);
  }
  p->context.size = size;
  return p;
}

void lc_free_packet(lch* mv, lc_packet* p)
{
  lc_pool_put(mv->pkpool, p);
}

void* lc_get_packet_data(lc_packet* p)
{
  return p->data.buffer;
}
