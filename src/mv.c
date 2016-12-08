// TODO(danghvu): Ugly hack to make thread-local storage faster.

#include <mpi.h>
#include "include/mv_priv.h"
#include <stdint.h>
#include "pool.h"

__thread int8_t
tls_pool_struct[MAX_LOCAL_POOL] = {-1, -1, -1, -1, -1, -1, -1, -1};

int mv_pool_nkey = 0;
uint32_t server_max_inline = 128;

uint8_t mv_am_register(mvh* mv, mv_am_func_t f)
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

  mv_hash_init(&mv->tbl);
  mv->am_table_size = 0;
  mv_progress_init(mv);
  mv_server_init(mv, heap_size, &mv->server);
  MPI_Barrier(MPI_COMM_WORLD);

  *ret = mv;
}

void mv_close(mvh* mv)
{
  MPI_Barrier(MPI_COMM_WORLD);
  mv_server_finalize(mv->server);
  MPI_Finalize();
}

void mv_send_rdz_post(mvh* mv, mv_ctx* ctx, mv_sync* sync)
{
  mvi_send_rdz_post(mv, ctx, sync);
}

void mv_send_rdz_init(mvh* mv, mv_ctx* ctx)
{
  mvi_send_rdz_init(mv, ctx);
}

void mv_send_rdz(mvh* mv, mv_ctx* ctx, mv_sync* sync)
{
  mvi_send_rdz_init(mv, ctx);
  mvi_send_rdz_post(mv, ctx, sync);
  mvi_wait(ctx, sync);
}

void mv_send_eager(mvh* mv, mv_ctx* ctx)
{
  mvi_send_eager(mv, ctx);
}

void mv_recv_rdz_init(mvh* mv, mv_ctx* ctx)
{
  mvi_recv_rdz_init(mv, ctx);
}

void mv_recv_rdz_post(mvh* mv, mv_ctx* ctx, mv_sync* sync)
{
  mvi_recv_rdz_post(mv, ctx, sync);
}

void mv_recv_rdz(mvh* mv, mv_ctx* ctx, mv_sync* sync)
{
  mvi_recv_rdz_init(mv, ctx);
  mvi_recv_rdz_post(mv, ctx, sync);
  mvi_wait(ctx, sync);
}

void mv_recv_eager_post(mvh* mv, mv_ctx* ctx, mv_sync* sync)
{
  mvi_recv_eager_post(mv, ctx, sync);
}

void mv_recv_eager(mvh* mv, mv_ctx* ctx, mv_sync* sync)
{
  mvi_recv_eager_post(mv, ctx, sync);
  mvi_wait(ctx, sync);
}

void mv_am_eager(mvh* mv, int node, void* src, int size,
                           uint32_t fid)
{
  mv_packet* p = (mv_packet*) mv_pool_get(mv->pkpool); 
  p->header.fid = PROTO_AM;
  p->header.from = mv->me;
  p->header.tag = fid;
  uint32_t* buffer = (uint32_t*)p->content.buffer;
  buffer[0] = size;
  memcpy((void*)&buffer[1], src, size);
  mv_server_send(mv->server, node, p,
                 sizeof(uint32_t) + (uint32_t)size + sizeof(packet_header),
                 p);
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
