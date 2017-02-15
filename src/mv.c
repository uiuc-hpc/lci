#include <mpi.h>
#include "include/mv_priv.h"
#include <stdint.h>
#include "pool.h"

MV_INLINE unsigned long find_first_set(unsigned long word)
{
  asm("rep; bsf %1,%0" : "=r"(word) : "rm"(word));
  return word;
}

__thread int mv_core_id = -1;
uintptr_t mv_comm_id[MAX_COMM_ID];
umalloc_heap_t* mv_heap = 0;

uint8_t mv_am_register(mvh* mv, mv_am_func_t f)
{
  mv->am_table[mv->am_table_size ++] = f;
  MPI_Barrier(MPI_COMM_WORLD);
  return mv->am_table_size - 1;
}

uint8_t mv_ps_register(mvh* mv, mv_am_func_t f)
{
  mv->am_table[mv->am_table_size ++] = f;
  MPI_Barrier(MPI_COMM_WORLD);
  return mv->am_table_size - 1;
}

void* mv_heap_ptr(mvh* mv)
{
  return mv_server_heap_ptr(mv->server);
}

void mv_open(int* argc, char*** args, size_t heap_size, mvh** ret)
{
  struct mv_struct* mv = malloc(sizeof(struct mv_struct));

  setenv("MPICH_ASYNC_PROGRESS", "0", 1);
  setenv("MV2_ASYNC_PROGRESS", "0", 1);
  setenv("MV2_ENABLE_AFFINITY", "0", 1);

  int provided;
  MPI_Init_thread(argc, args, MPI_THREAD_MULTIPLE, &provided);
  if (MPI_THREAD_MULTIPLE != provided) {
    printf("Need MPI_THREAD_MULTIPLE\n");
    exit(EXIT_FAILURE);
  }

  mv_pool_init();
  mv_hash_create(&mv->tbl);
  mv->am_table_size = 1;
  mv_server_init(mv, heap_size, &mv->server);

  // Init queue protocol.
#ifndef USE_CCQ
  dq_init(&mv->queue);
#else
  lcrq_init(&mv->queue);
#endif

  lcrq_init(&mv->squeue);

  // Init heap.
  mv_heap = umalloc_makeheap(mv_heap_ptr(mv), heap_size,
      UMALLOC_HEAP_GROWS_UP);

  // Prepare the list of packet.
  uint32_t npacket = MAX(MAX_PACKET, mv->size * 3);
  uintptr_t sbuf_addr = (uintptr_t) mv_alloc(MV_PACKET_SIZE * (2 + npacket));
  uintptr_t base_packet = (uintptr_t) sbuf_addr + 4096 - sizeof(struct packet_context);
  mv_pool_create(&mv->pkpool);
  for (unsigned i = 0; i < npacket; i++) {
    mv_packet* p = (mv_packet*) (base_packet + i * MV_PACKET_SIZE);
    p->context.pid = i;
    mv_pool_put(mv->pkpool, p);
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

int mv_send_init(mvh* mv, const void* src, int size, int rank, int tag, mv_ctx* ctx)
{
  if (size <= (int) SHORT_MSG_SIZE) {
    mv_packet* p = mv_pool_get_nb(mv->pkpool);
    if (!p) return 0;
    mvi_am_generic(mv, rank, src, size, tag, MV_PROTO_SHORT_MATCH, p);
    ctx->type = REQ_DONE;
  } else {
    mvi_send_rdz_init(mv, src, size, rank, tag, ctx);
  }
  return 1;
}

int mv_send_post(mvh* mv, mv_ctx* ctx, mv_sync* sync)
{
  if (!ctx || ctx->size <= (int) SHORT_MSG_SIZE)
    return 1;
  else
    return mvi_send_rdz_post(mv, ctx, sync);
}

int mv_recv_init(mvh* mv, void* src, int size, int rank, int tag, mv_ctx* ctx)
{
  if (size <= (int) SHORT_MSG_SIZE)
    mvi_recv_eager_init(mv, src, size, rank, tag, ctx);
  else {
    mv_packet* p = mv_pool_get_nb(mv->pkpool);
    if (!p) return 0;
    mvi_recv_rdz_init(mv, src, size, rank, tag, ctx, p);
  }
  return 1;
}

int mv_recv_post(mvh* mv, mv_ctx* ctx, mv_sync* sync)
{
  if (ctx->size <= (int) SHORT_MSG_SIZE)
    return mvi_recv_eager_post(mv, ctx, sync);
  else
    return mvi_recv_rdz_post(mv, ctx, sync);
}

int mv_send_enqueue_init(mvh* mv, const void* src, int size, int rank, int tag, mv_ctx* ctx)
{
  mv_packet* p = (mv_packet*) mv_pool_get_nb(mv->pkpool);
  if (!p) { ctx->type = REQ_NULL; return 0; };

  if (size <= (int) SHORT_MSG_SIZE) {
    mvi_am_generic(mv, rank, src, size, tag, MV_PROTO_SHORT_ENQUEUE, p);
    ctx->type = REQ_DONE;
  } else {
    INIT_CTX(ctx);
    p->data.content.rdz.sreq = (uintptr_t) ctx;
    p->context.poolid = mv_pool_get_local(mv->pkpool);
    p->data.header.proto = MV_PROTO_RTS_ENQUEUE;
    p->data.header.from = mv->me;
    p->data.header.tag = tag;
    p->data.header.size = size;
    mv_server_send(mv->server, rank, &p->data,
        (size_t)(sizeof(struct mv_rdz) + sizeof(struct packet_header)),
        &p->context);
  }
  return 1;
}

int mv_send_enqueue_post(mvh* mv __UNUSED__, mv_ctx* ctx, mv_sync *sync)
{
  if (ctx->type == REQ_DONE)
    return 1;
  else {
    ctx->sync = sync;
    return 0;
  }
}

int mv_recv_dequeue_init(mvh* mv, int* size, int* rank, int *tag, mv_ctx* ctx)
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
  return 1;
}

int mv_recv_dequeue_post(mvh* mv, void* buf, mv_ctx* ctx)
{
  mv_packet* p = (mv_packet*) ctx->packet;
  if (p->data.header.proto == MV_PROTO_SHORT_ENQUEUE) {
    memcpy(buf, p->data.content.buffer, p->data.header.size);
    mv_pool_put(mv->pkpool, p);
    ctx->type = REQ_DONE;
    return 1;
  } else {
    uint32_t comm_idx = p->context.pid;
    mv_comm_id[comm_idx] = (uintptr_t) ctx;
    int rank = p->data.header.from;
    p->data.header.from = mv->me;
    p->data.header.proto = MV_PROTO_RTR_ENQUEUE;
    p->data.content.rdz.tgt_addr = (uintptr_t) buf;
    p->data.content.rdz.comm_id = (uint32_t) comm_idx;
    mv_server_send(mv->server, rank, &p->data,
        sizeof(struct packet_header) + sizeof(struct mv_rdz), &p->context);
    ctx->type = REQ_PENDING;
    return 0;
  }
}

void mv_put(mvh* mv, int node, void* dst, void* src, int size)
{
  mv_server_rma(mv->server, node, src, dst,
      mv_server_heap_rkey(mv->server, node), size, 0);
}

void mv_put_signal(mvh* mv, int node, void* dst, void* src, int size,
    uint32_t sid)
{
  mv_server_rma_signal(mv->server, node, src, dst,
      mv_server_heap_rkey(mv->server, node), size, sid, 0);
}

void mv_progress(mvh* mv)
{
  mv_server_progress(mv->server);
}

size_t mv_data_max_size()
{
  return SHORT_MSG_SIZE;
}

// FIXME(danghvu): Get rid of this lock? or move to finer-grained.
volatile int ml = 0;

void* mv_alloc(size_t s)
{
  mv_spin_lock(&ml);
  void* p = umemalign(mv_heap, 4096, s);
  mv_spin_unlock(&ml);
  assert(p != 0 && "No more memory\n");
  return p;
}

void mv_free(void* ptr)
{
  mv_spin_lock(&ml);
  ufree(mv_heap, ptr);
  mv_spin_unlock(&ml);
}
