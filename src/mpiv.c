#include "mpiv.h"
#include "mv-inl.h"

mv_engine* mv_hdl;
uintptr_t MPIV_HEAP;

void MPIV_Recv(void* buffer, int count, MPI_Datatype datatype,
                         int rank, int tag, MPI_Comm comm, MPI_Status* status)
{
  int size;
  MPI_Type_size(datatype, &size);
  size *= count;
  mv_sync* sync = mv_get_sync();
  mv_ctx ctx = {
    .buffer = buffer,
    .size = size,
    .rank = rank,
    .tag = tag
  };
  if ((size_t)size <= SHORT_MSG_SIZE) {
    mv_recv_eager(mv_hdl, &ctx, sync);
  } else {
    mv_recv_rdz(mv_hdl, &ctx, sync);
  }
}

void MPIV_Send(void* buffer, int count, MPI_Datatype datatype,
                         int rank, int tag, MPI_Comm comm)
{
  int size;
  MPI_Type_size(datatype, &size);
  size *= count;
  mv_ctx ctx = {
     .buffer = buffer,
     .size = size,
     .rank = rank,
     .tag = tag
  };
  if (size <= SHORT_MSG_SIZE) {
    mv_send_eager(mv_hdl, &ctx);
  } else {
    mv_send_rdz(mv_hdl, &ctx, mv_get_sync());
  }
}

static void recv_eager_post(mv_engine* mv, mv_ctx* ctx, mv_sync* sync)
{
  mv_recv_eager_post(mv, ctx, sync);
}

static void recv_rdz_post(mv_engine* mv, mv_ctx* ctx, mv_sync* sync)
{
  mv_recv_rdz_post(mv, ctx, sync);
}

static void send_rdz_post(mv_engine* mv, mv_ctx* ctx, mv_sync* sync)
{
  mv_send_rdz_post(mv, ctx, sync);
}

void MPIV_Irecv(void* buffer, int count, MPI_Datatype datatype, int rank,
                int tag, MPI_Comm comm, MPIV_Request* req) {
  int size;
  MPI_Type_size(datatype, &size);
  size *= count;
  mv_ctx *ctx = malloc(sizeof(struct mv_ctx));
  ctx->buffer = buffer;
  ctx->size = size;
  ctx->rank = rank;
  ctx->tag = tag;
  if ((size_t)size <= SHORT_MSG_SIZE) {
    ctx->complete = recv_eager_post;
  } else {
    mv_recv_rdz_init(mv_hdl, ctx);
    ctx->complete = recv_rdz_post;
  }
  *req = (MPIV_Request) ctx;
}

void MPIV_Isend(const void* buf, int count, MPI_Datatype datatype, int rank,
                int tag, MPI_Comm comm, MPIV_Request* req) {
  int size;
  MPI_Type_size(datatype, &size);
  size *= count;
  mv_ctx *ctx = malloc(sizeof(struct mv_ctx));
  ctx->buffer = (void*) buf;
  ctx->size = size;
  ctx->rank = rank;
  ctx->tag = tag;

  if (size <= SHORT_MSG_SIZE) {
    mv_send_eager(mv_hdl, ctx);
    *req = MPI_REQUEST_NULL;
  } else {
    mv_send_rdz_init(mv_hdl, ctx);
    ctx->complete = send_rdz_post;
    *req = (MPIV_Request) ctx;
  }
}

void MPIV_Waitall(int count, MPIV_Request* req, MPI_Status* status) {
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

static int stop;
static pthread_t progress_thread;

static void* progress(void* arg)
{
  set_me_to_last();
  stop = 0;
  while (!stop) {
    while (mv_server_progress(mv_hdl->server))
      ;
  }
}

void MPIV_Init(int* argc, char*** args)
{
  size_t heap_size = 1024 * 1024 * 1024;
  mv_open(argc, args, heap_size, &mv_hdl);
  pthread_create(&progress_thread, 0, progress, 0);
  MPIV_HEAP = (uintptr_t) mv_heap_ptr(mv_hdl);
}

void MPIV_Finalize()
{
  stop = 1;
  pthread_join(progress_thread, 0);
  mv_close(mv_hdl);
}

void MPIV_Free(void* ptr) {}
