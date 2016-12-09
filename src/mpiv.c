#include "mv_priv.h"
#include "mv/affinity.h"

MV_EXPORT
mvh* mv_hdl;

void* MPIV_HEAP;

#define __UNUSED__ __attribute__((unused))

void MPIV_Recv(void* buffer, int count, MPI_Datatype datatype,
    int rank, int tag,
    MPI_Comm comm __UNUSED__,
    MPI_Status* status __UNUSED__)
{
  int size;
  MPI_Type_size(datatype, &size);
  size *= count;
  mv_sync* sync = mv_get_sync();
  struct mv_ctx ctx = {
    .buffer = buffer,
    .size = size,
    .rank = rank,
    .tag = tag
  };
  if ((size_t)size <= SHORT_MSG_SIZE) {
    mvi_recv_eager_post(mv_hdl, &ctx, sync);
  } else {
    mvi_recv_rdz_init(mv_hdl, &ctx);
    mvi_recv_rdz_post(mv_hdl, &ctx, sync);
  }
  mvi_wait(&ctx, sync);
}

void MPIV_Send(void* buffer, int count, MPI_Datatype datatype,
    int rank, int tag, MPI_Comm comm __UNUSED__)
{
  int size;
  MPI_Type_size(datatype, &size);
  size *= count;
  struct mv_ctx ctx = {
     .buffer = buffer,
     .size = size,
     .rank = rank,
     .tag = tag
  };
  if (size <= SHORT_MSG_SIZE) {
    mvi_send_eager(mv_hdl, &ctx);
  } else {
    mv_sync* sync = mv_get_sync();
    mvi_send_rdz_init(mv_hdl, &ctx);
    mvi_send_rdz_post(mv_hdl, &ctx, sync);
    mvi_wait(&ctx, sync);
  }
}

void MPIV_Irecv(void* buffer, int count, MPI_Datatype datatype, int rank,
                int tag, MPI_Comm comm __UNUSED__, MPIV_Request* req) {
  int size;
  MPI_Type_size(datatype, &size);
  size *= count;
  mv_ctx *ctx = malloc(sizeof(struct mv_ctx));
  ctx->buffer = buffer;
  ctx->size = size;
  ctx->rank = rank;
  ctx->tag = tag;
  if ((size_t)size <= SHORT_MSG_SIZE) {
    ctx->complete = mv_recv_eager_post;
  } else {
    mvi_recv_rdz_init(mv_hdl, ctx);
    ctx->complete = mv_recv_rdz_post;
  }
  *req = (MPIV_Request) ctx;
}

void MPIV_Isend(const void* buf, int count, MPI_Datatype datatype, int rank,
                int tag, MPI_Comm comm __UNUSED__, MPIV_Request* req) {
  int size;
  MPI_Type_size(datatype, &size);
  size *= count;
  mv_ctx *ctx = malloc(sizeof(struct mv_ctx));
  ctx->buffer = (void*) buf;
  ctx->size = size;
  ctx->rank = rank;
  ctx->tag = tag;

  if (size <= SHORT_MSG_SIZE) {
    mvi_send_eager(mv_hdl, ctx);
    *req = MPI_REQUEST_NULL;
  } else {
    mvi_send_rdz_init(mv_hdl, ctx);
    ctx->complete = mv_send_rdz_post;
    *req = (MPIV_Request) ctx;
  }
}

void MPIV_Waitall(int count, MPIV_Request* req, MPI_Status* status __UNUSED__) {
  mv_sync* counter = mv_get_counter(count);
  for (int i = 0; i < count; i++) {
    if (req[i] == MPI_REQUEST_NULL) {
      thread_signal(counter);
      continue;
    }
    mv_ctx* ctx = (mv_ctx *) req[i];
    ctx->complete(mv_hdl, ctx, counter);
    if (ctx->type == REQ_DONE) {
      thread_signal(counter);
    }
  }
  thread_wait(counter);
  for (int i = 0; i < count; i++) {
    if (req[i] != MPI_REQUEST_NULL) {
      mv_ctx* ctx = (mv_ctx*) req[i];
      free(ctx);
      req[i] = MPI_REQUEST_NULL;
    }
  }
}

volatile int stop;
static pthread_t progress_thread;

static void* progress(void* arg __UNUSED__)
{
  set_me_to_last();
  while (!stop) {
    while (mv_server_progress(mv_hdl->server))
      ;
  }
  return 0;
}

void MPIV_Init(int* argc, char*** args)
{
  size_t heap_size = 1024 * 1024 * 1024;
  mv_open(argc, args, heap_size, &mv_hdl);
  stop = 0;
  pthread_create(&progress_thread, 0, progress, 0);
  MPIV_HEAP = mv_heap_ptr(mv_hdl);
}

void MPIV_Finalize()
{
  stop = 1;
  pthread_join(progress_thread, 0);
  mv_close(mv_hdl);
}

void* MPIV_Alloc(size_t size)
{
    void* ptr = MPIV_HEAP; 
    MPIV_HEAP = (char*) MPIV_HEAP + size;
    return ptr;
}

void MPIV_Free(void* ptr __UNUSED__) {}