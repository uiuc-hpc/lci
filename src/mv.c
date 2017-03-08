#include <mpi.h>
#include "include/mv_priv.h"
#include <stdint.h>
#include "pool.h"

#include "dreg/dreg.h"

size_t server_max_inline;
__thread int mv_core_id = -1;

void* mv_heap_ptr(mvh* mv)
{
  return mv_server_heap_ptr(mv->server);
}

void mv_open(int* argc, char*** args, size_t heap_size, mvh** ret)
{
  struct mv_struct* mv = 0;
  posix_memalign((void**) &mv, 4096, sizeof(struct mv_struct));

  setenv("MPICH_ASYNC_PROGRESS", "0", 1);
  setenv("MV2_ASYNC_PROGRESS", "0", 1);
  setenv("MV2_ENABLE_AFFINITY", "0", 1);
  setenv("MV2_USE_LAZY_MEM_UNREGISTER", "0", 1);

  int provided;

  MPI_Init_thread(argc, args, MPI_THREAD_MULTIPLE, &provided);
  if (MPI_THREAD_MULTIPLE != provided) {
    printf("Need MPI_THREAD_MULTIPLE\n");
    exit(EXIT_FAILURE);
  }

  mv_pool_init();
  mv_hash_create(&mv->tbl);
  // mv->am_table_size = 1;

  mv_server_init(mv, heap_size, &mv->server);

  // Init queue protocol.
#ifndef USE_CCQ
  dq_init(&mv->queue);
#else
  lcrq_init(&mv->queue);
#endif

  // Prepare the list of packet.
  uint32_t npacket = MAX(MAX_PACKET, mv->size * 4);
  uintptr_t sbuf_addr = (uintptr_t) mv_heap_ptr(mv);
  uintptr_t base_packet = (uintptr_t) sbuf_addr + 4096;
  mv_pool_create(&mv->pkpool);

  for (unsigned i = 0; i < npacket; i++) {
    mv_packet* p = (mv_packet*) (base_packet + i * MV_PACKET_SIZE);
    p->context.poolid = 0; // default to the first pool -- usually server.
    mv_pool_put(mv->pkpool, p);
  }

  // Prepare the list of rma_ctx.
  uintptr_t base_rma = (base_packet + npacket * MV_PACKET_SIZE);
  mv_pool_create(&mv->rma_pool);
  for(unsigned i = 0; i < MAX_PACKET; i++) {
    struct mv_rma_ctx* rma_ctx = (struct mv_rma_ctx*) (base_rma + i * sizeof(struct mv_rma_ctx));
    mv_pool_put(mv->rma_pool, rma_ctx);
  }

  MPI_Barrier(MPI_COMM_WORLD);
  *ret = mv;
}

void mv_close(mvh* mv)
{
  MPI_Barrier(MPI_COMM_WORLD);
  mv_server_finalize(mv->server);
  mv_hash_destroy(mv->tbl);
  mv_pool_destroy(mv->pkpool);
#ifdef USE_CCQ
  lcrq_destroy(&mv->queue);
#endif
  free(mv);
  MPI_Finalize();
}

int mv_send(mvh* mv, const void* src, int size, int rank, int tag, mv_ctx* ctx)
{
  return mvi_send(mv, src, size, rank, tag, ctx);
}

int mv_send_post(mvh* mv, mv_ctx* ctx, mv_sync* sync)
{
  return mvi_send_post(mv, ctx, sync);
}

int mv_recv(mvh* mv, void* src, int size, int rank, int tag, mv_ctx* ctx)
{
  return mvi_recv(mv, src, size, rank, tag, ctx);
}

int mv_recv_post(mvh* mv, mv_ctx* ctx, mv_sync* sync)
{
  return mvi_recv_post(mv, ctx, sync);
}

void mv_send_persis(mvh* mv, mv_packet* p, int rank, mv_ctx* ctx)
{
  p->context.req = (uintptr_t) ctx;
  mv_server_send(mv->server, rank, &p->data,
      (size_t)(p->data.header.size + sizeof(struct packet_header)),
      p, MV_PROTO_PERSIS);
}

int mv_send_queue(mvh* mv, const void* src, int size, int rank, int tag, mv_ctx* ctx)
{
  mv_packet* p = (mv_packet*) mv_pool_get_nb(mv->pkpool);
  if (!p) { ctx->type = REQ_NULL; return 0; };
  if (size <= (int) SHORT_MSG_SIZE) {
    mvi_am_generic(mv, rank, src, size, tag, MV_PROTO_SHORT_QUEUE, p);
    ctx->type = REQ_DONE;
  } else {
    INIT_CTX(ctx);
    p->data.rdz.sreq = (uintptr_t) ctx;
    p->data.header.from = mv->me;
    p->data.header.tag = tag;
    p->data.header.size = size;
    mv_server_send(mv->server, rank, &p->data,
        (size_t)(sizeof(struct mv_rdz) + sizeof(struct packet_header)),
        p, MV_PROTO_RTS_QUEUE);
  }
  return 1;
}

int mv_recv_queue(mvh* mv, int* size, int* rank, int *tag, mv_ctx* ctx)
{
#ifndef USE_CCQ
  mv_packet* p = (mv_packet*) dq_pop_bot(&mv->queue);
#else
  mv_packet* p = (mv_packet*) lcrq_dequeue(&mv->queue);
#endif
  if (p == NULL) return 0;

  *rank = p->data.header.from;
  *tag = p->data.header.tag;
  *size = p->data.header.size;
  ctx->packet = p;
  ctx->type = REQ_PENDING;

  return 1;
}

int mv_recv_queue_post(mvh* mv, void* buf, mv_ctx* ctx)
{
  mv_packet* p = (mv_packet*) ctx->packet;
  if (p->context.proto != MV_PROTO_RTS_QUEUE) {
    memcpy(buf, p->data.buffer, p->data.header.size);
    mv_pool_put(mv->pkpool, p);
    ctx->type = REQ_DONE;
    return 1;
  } else {
    int rank = p->data.header.from;
    p->context.req = (uintptr_t) ctx;
    uintptr_t rma_mem = mv_server_rma_reg(mv->server, buf, p->data.header.size);
    p->context.rma_mem = rma_mem;
    p->data.rdz.tgt_addr = (uintptr_t) buf;
    p->data.rdz.rkey = mv_server_rma_key(rma_mem);
    p->data.rdz.comm_id = (uint32_t) ((uintptr_t) p - (uintptr_t) mv_heap_ptr(mv));
    mv_server_send(mv->server, rank, &p->data,
        sizeof(struct packet_header) + sizeof(struct mv_rdz), p, MV_PROTO_RTR_QUEUE);
    return 0;
  }
}

int mv_send_put(mvh* mv, void* src, int size, int rank, mv_addr* dst, mv_ctx* ctx)
{
  mv_packet* p = (mv_packet*) mv_pool_get_nb(mv->pkpool);
  if (!p) { ctx->type = REQ_NULL; return 0; };
  struct mv_rma_ctx* dctx = (struct mv_rma_ctx*) dst;
  ctx->type = REQ_PENDING;
  p->context.req = (uintptr_t) ctx;
  mv_server_rma(mv->server, rank, src, dctx->addr, dctx->rkey, size, p, MV_PROTO_LONG_PUT);
  return 1;
}

int mv_send_put_signal(mvh* mv, void* src, int size, int rank, mv_addr* dst, mv_ctx* ctx)
{
  mv_packet* p = (mv_packet*) mv_pool_get_nb(mv->pkpool);
  if (!p) { ctx->type = REQ_NULL; return 0; };
  struct mv_rma_ctx* dctx = (struct mv_rma_ctx*) dst;
  ctx->type = REQ_PENDING;
  p->context.req = (uintptr_t) ctx;
  mv_server_rma_signal(mv->server, rank, src, dctx->addr, dctx->rkey, size,
      RMA_SIGNAL_SIMPLE | dctx->sid, p, MV_PROTO_LONG_PUT);
  return 1;
}

int mv_rma_create(mvh* mv, void* buf, size_t size, mv_addr** rctx_ptr)
{
  struct mv_rma_ctx* rctx = (struct mv_rma_ctx*) mv_pool_get(mv->rma_pool);
  uintptr_t rma = mv_server_rma_reg(mv->server, buf, size);
  rctx->addr = (uintptr_t) buf;
  rctx->rkey = mv_server_rma_key(rma);
  rctx->sid = (uint32_t) ((uintptr_t) rctx - (uintptr_t) mv_heap_ptr(mv));
  *rctx_ptr = rctx;
  return 1;
}

int mv_recv_put_signal(mvh* mv, mv_addr* rctx, mv_ctx* ctx)
{
  rctx->req = (uintptr_t) ctx;
  ctx->type = REQ_PENDING;
  return 1;
}

void mv_progress(mvh* mv)
{
  mv_server_progress(mv->server);
}

size_t mv_get_ncores()
{
  return sysconf(_SC_NPROCESSORS_ONLN) / THREAD_PER_CORE;
}

mv_packet* mv_alloc_packet(mvh* mv, int tag, int size)
{
  mv_packet* p = mv_pool_get_nb(mv->pkpool);
  if (!p) return NULL;
  if (size >= (int) SHORT_MSG_SIZE) {
    fprintf(stderr, "Message size %d too big, try < %d\n", size, (int) SHORT_MSG_SIZE);
    exit(EXIT_FAILURE);
  }
  p->data.header.from = mv->me;
  p->data.header.size = size;
  p->data.header.tag = tag;
  return p;
}

void mv_free_packet(mvh* mv, mv_packet* p)
{
  mv_pool_put(mv->pkpool, p);
}

void* mv_get_packet_data(mv_packet* p)
{
  return p->data.buffer;
}
