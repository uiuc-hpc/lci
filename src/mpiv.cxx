#include "mpiv.h"

mv_engine* mv_hdl;
uintptr_t MPIV_HEAP;

// TODO(danghvu): Ugly hack to make thread-local storage faster.
__thread int8_t
tls_pool_struct[MAX_LOCAL_POOL] = {-1, -1, -1, -1, -1, -1, -1, -1};

int mv_pool_nkey = 0;

int PROTO_SHORT;
int PROTO_RECV_READY;
int PROTO_READY_FIN;
int PROTO_AM;
int PROTO_SEND_WRITE_FIN = 99;

void MPIV_Recv(void* buffer, int count, MPI_Datatype datatype,
                         int rank, int tag, MPI_Comm, MPI_Status*)
{
  int size;
  MPI_Type_size(datatype, &size);
  size *= count;
  mv_sync* sync = mv_get_sync();
  mv_ctx ctx(buffer, size, rank, tag);
  if ((size_t)size <= SHORT_MSG_SIZE) {
    mv_recv_eager(mv_hdl, &ctx, sync);
  } else {
    mv_recv_rdz(mv_hdl, &ctx, sync);
  }
}

void MPIV_Send(void* buffer, int count, MPI_Datatype datatype,
                         int rank, int tag, MPI_Comm)
{
  int size;
  MPI_Type_size(datatype, &size);
  size *= count;
  mv_ctx ctx(buffer, size, rank, tag);
  if (size <= SHORT_MSG_SIZE) {
    mv_send_eager(mv_hdl, &ctx);
  } else {
    mv_send_rdz(mv_hdl, &ctx, mv_get_sync());
  }
}


void MPIV_Irecv(void* buffer, int count, MPI_Datatype datatype, int rank,
                int tag, MPI_Comm, MPIV_Request* req) {
  int size;
  MPI_Type_size(datatype, &size);
  size *= count;
  mv_ctx *ctx = new mv_ctx(buffer, size, rank, tag);
  if ((size_t)size <= SHORT_MSG_SIZE) {
    ctx->complete = mv_recv_eager_post;
  } else {
    mv_recv_rdz_init(mv_hdl, ctx);
    ctx->complete = mv_recv_rdz_post;
  }
  *req = (MPIV_Request) ctx;
}

void MPIV_Isend(const void* buf, int count, MPI_Datatype datatype, int rank,
                int tag, MPI_Comm, MPIV_Request* req) {
  int size;
  MPI_Type_size(datatype, &size);
  size *= count;
  mv_ctx *ctx = new mv_ctx((void*) buf, size, rank, tag);
  if (size <= SHORT_MSG_SIZE) {
    mv_send_eager(mv_hdl, ctx);
    *req = MPI_REQUEST_NULL;
  } else {
    mv_send_rdz_init(mv_hdl, ctx);
    ctx->complete = mv_send_rdz_post;
    *req = (MPIV_Request) ctx;
  }
}

void MPIV_Waitall(int count, MPIV_Request* req, MPI_Status*) {
  mv_sync* counter = mv_get_counter(count);
  for (int i = 0; i < count; i++) {
    if (req[i] == MPI_REQUEST_NULL) {
      thread_signal(counter);
      continue;
    }
    mv_ctx* ctx = (mv_ctx *) req[i];
    ctx->complete(mv_hdl, ctx, counter);
    if (ctx->type == REQ_DONE) {
      req[i] = MPI_REQUEST_NULL;
      thread_signal(counter);
      delete ctx;
    }
  }
  thread_wait(counter);
  for (int i = 0; i < count; i++) {
    req[i] = MPI_REQUEST_NULL;
  }
}

static bool stop;
static std::thread progress_thread;

static void progress(mv_engine* mv)
{
  affinity::set_me_to_last();
  stop = false;
  while (!stop) {
    mv_progress(mv);
  }
}

void MPIV_Init(int* argc, char*** args)
{
  size_t heap_size = 1024 * 1024 * 1024;
  mv_open(argc, args, heap_size, &mv_hdl);
  progress_thread = std::move(std::thread(progress, mv_hdl));
  MPIV_HEAP = (uintptr_t) mv_heap_ptr(mv_hdl);
}

void MPIV_Finalize()
{
  stop = true;
  progress_thread.join();
  mv_close(mv_hdl);
}
