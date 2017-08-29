#include <assert.h>

#include "mpiv.h"
#include "lc_priv.h"
#include "lc/affinity.h"
#include "lc/macro.h"
#include "thread.h"

static void* ctx_data;
static lc_pool* lc_req_pool;
LC_EXPORT
lch* lc_hdl;

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
  lc_req ctx;
  while (!lc_recv_tag(lc_hdl, buffer, size, rank, tag, &ctx))
    g_sync.yield();
  lc_sync sync = {0, 0, -1};
  lc_wait(lc_hdl, &ctx, &sync);
}

void MPI_Send(void* buffer, int count, MPI_Datatype datatype,
    int rank, int tag, MPI_Comm lc_hdl __UNUSED__)
{
  int size = 1;
  MPI_Type_size(datatype, &size);
  size *= count;
  lc_req ctx;
  while (!lc_send_tag(lc_hdl, buffer, size, rank, tag, &ctx))
    g_sync.yield();
  lc_sync sync = {0, 0, -1};
  lc_wait(lc_hdl, &ctx, &sync);
}

void MPI_Ssend(void* buffer, int count, MPI_Datatype datatype,
    int rank, int tag, MPI_Comm lc_hdl __UNUSED__)
{
  int size;
  MPI_Type_size(datatype, &size);
  size *= count;
  lc_req ctx;
  while (!lc_send_tag(lc_hdl, buffer, size, rank, tag, &ctx))
    g_sync.yield();
  lc_sync sync = {0, 0, -1};
  lc_wait(lc_hdl, &ctx, &sync);
}

void MPI_Isend(const void* buf, int count, MPI_Datatype datatype, int rank,
                int tag, MPI_Comm lc_hdl __UNUSED__, MPI_Request* req) {
  int size;
  MPI_Type_size(datatype, &size);
  size *= count;
  lc_req *ctx = (lc_req*) lc_pool_get(lc_req_pool);
  while (!lc_send_tag(lc_hdl, buf, size, rank, tag, ctx))
    g_sync.yield();
  if (ctx->type != LC_REQ_DONE) {
    *req = (MPI_Request) ctx;
  } else {
    lc_pool_put(lc_req_pool, ctx);
    *req = MPI_REQUEST_NULL;
  }
}

void MPI_Irecv(void* buffer, int count, MPI_Datatype datatype, int rank,
                int tag, MPI_Comm lc_hdl __UNUSED__, MPI_Request* req) {
  int size;
  MPI_Type_size(datatype, &size);
  size *= count;
  lc_req *ctx = (lc_req*) lc_pool_get(lc_req_pool);
  while (!lc_recv_tag(lc_hdl, (void*) buffer, size, rank, tag, ctx))
    g_sync.yield();
  *req = (MPI_Request) ctx;
}

void MPI_Waitall(int count, MPI_Request* req, MPI_Status* status __UNUSED__) {
  int pending = count;
  for (int i = 0; i < count; i++) {
    if (req[i] == MPI_REQUEST_NULL)
      pending--;
  }
  lc_sync sync = {0, 0, pending};

  for (int i = 0; i < count; i++) {
    if (req[i] != MPI_REQUEST_NULL) {
      lc_req* ctx = (lc_req *) req[i];
      if (lc_post(ctx, &sync) == LC_OK) {
        __sync_fetch_and_sub(&sync.count, 1);
        lc_pool_put(lc_req_pool, ctx);
        req[i] = MPI_REQUEST_NULL;
      }
    }
  }
  for (int i = 0; i < count; i++) {
    if (req[i] != MPI_REQUEST_NULL) {
      lc_req* ctx = (lc_req*) req[i];
      lc_sync_wait(&sync, &ctx->int_type);
      lc_pool_put(lc_req_pool, ctx);
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
  // setenv("LC_MPI", "1", 1);
  lc_open(&lc_hdl);
  ctx_data = memalign(64, sizeof(lc_req) * MAX_PACKET);
  lc_pool_create(&lc_req_pool);
  lc_req* ctxs = (lc_req*) ctx_data;
  for (int i = 0; i < MAX_PACKET; i++)
    lc_pool_put(lc_req_pool, &ctxs[i]);
  lc_thread_stop = 0;
  pthread_create(&progress_thread, 0, progress, (void*) (long)lc_hdl->me);
}

void MPI_Finalize()
{
  lc_thread_stop = 1;
  pthread_join(progress_thread, 0);
  free(ctx_data);
  lc_close(lc_hdl);
}
