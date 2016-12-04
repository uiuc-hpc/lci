#ifndef MPIV_MPIV_H_
#define MPIV_MPIV_H_

#include "mv-inl.h"
#include "mv.h"
#include "request.h"
#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/managed_external_buffer.hpp>
#include <mpi.h>

typedef uintptr_t MPIV_Request;

typedef boost::interprocess::basic_managed_external_buffer<
    char, boost::interprocess::rbtree_best_fit<
              boost::interprocess::mutex_family, void*, 64>,
    boost::interprocess::iset_index>
    mbuffer;

extern mv_engine* mv_hdl;
extern mbuffer heap_segment;

MV_INLINE void MPIV_Recv(void* buffer, int count, MPI_Datatype datatype,
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

MV_INLINE void MPIV_Send(void* buffer, int count, MPI_Datatype datatype,
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

MV_INLINE void MPIV_Irecv(void* buffer, int count, MPI_Datatype datatype, int rank,
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

MV_INLINE void MPIV_Isend(const void* buf, int count, MPI_Datatype datatype, int rank,
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

MV_INLINE void MPIV_Waitall(int count, MPIV_Request* req, MPI_Status*) {
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

MV_INLINE void MPIV_Init(int* argc, char*** args)
{
  size_t heap_size = 1024 * 1024 * 1024;
  mv_open(argc, args, heap_size, &mv_hdl);
  heap_segment = std::move(mbuffer(boost::interprocess::create_only,
                                   mv_heap_ptr(mv_hdl), heap_size));
}

MV_INLINE void MPIV_Finalize() { mv_close(mv_hdl); }
MV_INLINE void* MPIV_Alloc(int size) { return heap_segment.allocate(size); }
MV_INLINE void MPIV_Free(void* ptr) { return heap_segment.deallocate(ptr); }
#endif
