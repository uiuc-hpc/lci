#include "mv.h"
#include "mv-inl.h"

MV_INLINE void proto_recv_rndz(mv_engine* mv, void* buffer, int size, int rank, int tag,
                     MPIV_Request* s);

void mv_irecv(mv_engine* mv, void* buffer, size_t size, int rank, int tag,
           MPIV_Request* req) {
  if (size <= SHORT_MSG_SIZE) 
    req->type = REQ_RECV_SHORT;
  else {
    req->type = REQ_RECV_LONG;
    // proto_recv_rndz(mv, buffer, size, rank, tag, req);
  }
}
