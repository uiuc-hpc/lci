#include "mpiv.h"

#include "lc_priv.h"
#include "lc/affinity.h"
#include "lc/macro.h"
#include "ult/helper.h"

static void* ctx_data;
static lc_pool* lc_ctx_pool;
LC_EXPORT
lch* lc_hdl;

void* MPI_HEAP;

static inline void MPI_Type_size(MPI_Datatype type, int *size)
{
  *size = (int) type;
}

void MPI_Recv(void* buffer, int count, MPI_Datatype datatype,
    int rank, int tag,
    MPI_Comm lc_hdl,
    MPI_Status* status __UNUSED__)
{
  int size = 1;
  MPI_Type_size(datatype, &size);
  size *= count;
  struct lc_ctx ctx;
  while (!lc_recv_tag(lc_hdl, buffer, size, rank, tag, &ctx))
    thread_yield();
  lc_sync* sync = lc_get_sync();
  lc_recv_tag_post(lc_hdl, &ctx, sync);
  lc_wait(&ctx, sync);
}

void MPI_Send(void* buffer, int count, MPI_Datatype datatype,
    int rank, int tag, MPI_Comm lc_hdl __UNUSED__)
{
  int size = 1;
  MPI_Type_size(datatype, &size);
  size *= count;
  struct lc_ctx ctx;
  while (!lc_send_tag(lc_hdl, buffer, size, rank, tag, &ctx))
    thread_yield();
  lc_sync* sync = lc_get_sync();
  lc_send_tag_post(lc_hdl, &ctx, sync);
  lc_wait(&ctx, sync);
}

void MPI_Ssend(void* buffer, int count, MPI_Datatype datatype,
    int rank, int tag, MPI_Comm lc_hdl __UNUSED__)
{
  int size;
  MPI_Type_size(datatype, &size);
  size *= count;
  struct lc_ctx ctx;
  while (!lc_send_tag(lc_hdl, buffer, size, rank, tag, &ctx))
    thread_yield();
  lc_sync* sync = lc_get_sync();
  lc_send_tag_post(lc_hdl, &ctx, sync);
  lc_wait(&ctx, sync);
}

void MPI_Isend(const void* buf, int count, MPI_Datatype datatype, int rank,
                int tag, MPI_Comm lc_hdl __UNUSED__, MPI_Request* req) {
  int size;
  MPI_Type_size(datatype, &size);
  size *= count;
  lc_ctx *ctx = (lc_ctx*) lc_pool_get(lc_ctx_pool);
  while (!lc_send_tag(lc_hdl, buf, size, rank, tag, ctx))
    thread_yield();
  if (ctx->type != REQ_DONE) {
    ctx->complete = lc_send_tag_post;
    *req = (MPI_Request) ctx;
  } else {
    lc_pool_put(lc_ctx_pool, ctx);
    *req = MPI_REQUEST_NULL;
  }
}

void MPI_Irecv(void* buffer, int count, MPI_Datatype datatype, int rank,
                int tag, MPI_Comm lc_hdl __UNUSED__, MPI_Request* req) {
  int size;
  MPI_Type_size(datatype, &size);
  size *= count;
  lc_ctx *ctx = (lc_ctx*) lc_pool_get(lc_ctx_pool);
  while (!lc_recv_tag(lc_hdl, (void*) buffer, size, rank, tag, ctx))
    thread_yield();
  ctx->complete = lc_recv_tag_post;
  *req = (MPI_Request) ctx;
}

void MPI_Waitall(int count, MPI_Request* req, MPI_Status* status __UNUSED__) {
  int pending = count;
  for (int i = 0; i < count; i++) {
    if (req[i] == MPI_REQUEST_NULL)
      pending--;
  }
  lc_sync* counter = lc_get_counter(pending);
  for (int i = 0; i < count; i++) {
    if (req[i] != MPI_REQUEST_NULL) {
      lc_ctx* ctx = (lc_ctx *) req[i];
      if (ctx->complete(lc_hdl, ctx, counter)) {
        thread_signal(counter);
      }
    }
  }
  for (int i = 0; i < count; i++) {
    if (req[i] != MPI_REQUEST_NULL) {
      lc_ctx* ctx = (lc_ctx*) req[i];
      lc_wait(ctx, counter);
      lc_pool_put(lc_ctx_pool, ctx);
      req[i] = MPI_REQUEST_NULL;
    }
  }
}

volatile int lc_thread_stop;
static pthread_t progress_thread;

static void* progress(void* arg __UNUSED__)
{
  int c = 0;
  if (getenv("LC_POLL_CORE"))
    c = atoi(getenv("LC_POLL_CORE"));
  set_me_to(c);
  while (!lc_thread_stop) {
    while (lc_progress(lc_hdl))
      ;
  }
  return 0;
}

void MPI_Init(int* argc __UNUSED__, char*** args __UNUSED__)
{
  size_t heap_size = 256 * 1024 * 1024;
  // setenv("LC_MPI", "1", 1);
  lc_open(heap_size, &lc_hdl);
  posix_memalign(&ctx_data, 64, sizeof(struct lc_ctx) * MAX_PACKET);
  lc_pool_create(&lc_ctx_pool);
  lc_ctx* ctxs = (lc_ctx*) ctx_data;
  for (int i = 0; i < MAX_PACKET; i++)
    lc_pool_put(lc_ctx_pool, &ctxs[i]);
  lc_thread_stop = 0;
  pthread_create(&progress_thread, 0, progress, (void*) (long)lc_hdl->me);
  MPI_HEAP = lc_heap_ptr(lc_hdl);
}

void MPI_Finalize()
{
  lc_thread_stop = 1;
  pthread_join(progress_thread, 0);
  free(ctx_data);
  lc_close(lc_hdl);
}
