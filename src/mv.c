#include <mpi.h>
#include "include/mv_priv.h"
#include <stdint.h>
#include "pool.h"
#include "umalloc/umalloc.h"

__thread int mv_core_id = -1;
uint32_t server_max_inline = 128;
double mv_ptime  = 0;
extern uint8_t MV_AM_GENERIC;

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

#if 1
  for (int i = 0; i < MAX_NPOOLS; i++) {
    for (int j = 0; j < MAX_LOCAL_POOL; j++) {
      tls_pool_struct[i][j] = -1;
    }
  }
#endif
  mv_hash_init(&mv->tbl);
  mv->am_table_size = 1;
  mv_server_init(mv, heap_size, &mv->server);
  MPI_Barrier(MPI_COMM_WORLD);

  mv->heap = umalloc_makeheap(mv_heap_ptr(mv), heap_size,
      UMALLOC_HEAP_GROWS_UP);
  dq_init(&mv->queue, MAX_PACKET);
  *ret = mv;
}

void mv_close(mvh* mv)
{
  MPI_Barrier(MPI_COMM_WORLD);
  mv_server_finalize(mv->server);
  free(mv);
  MPI_Finalize();
}

void mv_send_init(mvh* mv, void* src, int size, int rank, int tag, mv_ctx* ctx)
{
  if (size <= (int) SHORT_MSG_SIZE) {
    mvi_send_eager(mv, src, size, rank, tag);
    ctx->type = REQ_DONE;
  } else {
    mvi_send_rdz_init(mv, src, size, rank, tag, ctx);
  }
}

int mv_send_post(mvh* mv, mv_ctx* ctx, mv_sync* sync)
{
  if (ctx->size <= (int) SHORT_MSG_SIZE) 
    return 1;
  else
    return mvi_send_rdz_post(mv, ctx, sync);
}

void mv_recv_init(mvh* mv, void* src, int size, int rank, int tag, mv_ctx* ctx)
{
  if (size <= (int) SHORT_MSG_SIZE) 
    mvi_recv_eager_init(mv, src, size, rank, tag, ctx);
  else
    mvi_recv_rdz_init(mv, src, size, rank, tag, ctx);
}

int mv_recv_post(mvh* mv, mv_ctx* ctx, mv_sync* sync)
{
  if (ctx->size <= (int) SHORT_MSG_SIZE) 
    return mvi_recv_eager_post(mv, ctx, sync);
  else
    return mvi_recv_rdz_post(mv, ctx, sync);
}

#if 0
void mv_am_eager(mvh* mv, int node, void* src, int size, int tag,
                 uint8_t fid)
{
  mv_packet* p = (mv_packet*) mv_pool_get(mv->pkpool); 
  mvi_am_generic2(mv, node, src, size, tag, fid, 0, p);
}

void mv_am_eager2(mvh* mv, int node, void* src, int size, int tag,
                 uint8_t am_fid, uint8_t ps_fid, mv_packet* p)
{
  mvi_am_generic2(mv, node, src, size, tag, am_fid, ps_fid, p);
}
#endif

int mv_send_enqueue_init(mvh* mv, void* src, int size, int rank, int tag, mv_ctx* ctx)
{
  mv_packet* p = (mv_packet*) mv_pool_get_nb(mv->pkpool); 
  if (!p) return 0;

  if (size <= (int) SHORT_MSG_SIZE) {
    mvi_am_generic(mv, rank, src, size, tag, MV_PROTO_SHORT_ENQUEUE, p);
    ctx->type = REQ_DONE;
  } else {
    INIT_CTX(ctx);
    mv_set_proto(p, MV_PROTO_RTS);
    p->data.header.poolid = 0;
    p->data.header.from = mv->me;
    p->data.header.tag = ctx->tag;
    p->data.header.size = ctx->size;
    p->data.content.rdz.sreq = (uintptr_t) ctx;
    mv_server_send(mv->server, ctx->rank, &p->data,
        sizeof(packet_header) + sizeof(struct mv_rdz), &p->context);
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

int mv_recv_dequeue(mvh* mv, mv_ctx* ctx)
{
  mv_packet* p = (mv_packet*) dq_pop_bot(&mv->queue);
  if (p == NULL) return 0;
  ctx->rank = p->data.header.from;
  ctx->tag = p->data.header.tag;
  ctx->size = p->data.header.size;
  if (p->data.header.proto == MV_PROTO_SHORT_ENQUEUE) {
    // TODO(danghvu): Have to do this to return the packet.
    ctx->buffer = (void*) mv_alloc(mv, ctx->size);
    memcpy(ctx->buffer, p->data.content.buffer, ctx->size);
  } else {
    ctx->buffer = (void*) p->data.content.rdz.tgt_addr;
  }
  mv_packet_done(mv, p);
  return 1;
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

mv_packet_data_t* mv_packet_data(mv_packet* p)
{
  return &p->data;
}

void mv_packet_done(mvh* mv, mv_packet* p)
{
  mv_pool_put(mv->pkpool, p);
}

size_t mv_data_max_size()
{
  return SHORT_MSG_SIZE;
}

static volatile int memlock = 0;

void* mv_alloc(mvh* mv, size_t s)
{
  mv_spin_lock(&memlock);
  void* p = umalloc(mv->heap, s);
  mv_spin_unlock(&memlock);
  if (p == 0) {
    fprintf(stderr, "Not enough memory for allocation");
    exit(EXIT_FAILURE);
  }
  return p;
}

void mv_free(mvh* mv, void* ptr)
{
  mv_spin_lock(&memlock);
  ufree(mv->heap, ptr);
  mv_spin_unlock(&memlock);
}
