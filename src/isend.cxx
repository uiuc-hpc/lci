#include "mv.h"
#include "mv-inl.h"

MV_INLINE void proto_send_rdz(mv_engine* mv, MPIV_Request* s);
MV_INLINE void proto_send_short(mv_engine* mv, const void* buffer, int size, int rank, int tag);

void mv_isend(mv_engine* mv, const void* buf, size_t size, int rank, int tag, MPIV_Request* req) {
  if (size <= SHORT_MSG_SIZE) {
    req->type = REQ_NULL;
    proto_send_short(mv, buf, size, rank, tag);
  } else {
    new (req) MPIV_Request((void*)buf, size, rank, tag);
    req->type = REQ_SEND_LONG;
    proto_send_rdz(mv, req);
  }
}
