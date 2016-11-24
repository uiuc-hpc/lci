#include "mv.h"
#include "mv-inl.h"

MV_INLINE void proto_send_rdz(mv_engine* mv, MPIV_Request* s);
MV_INLINE void proto_send_eager(mv_engine* mv, const void* buffer, int size, int rank, int tag);

void mv_isend(mv_engine* mv, const void* buf, size_t size, int rank, int tag, MPIV_Request* req) {
  if (size <= SHORT_MSG_SIZE) {
    mv_send_eager(mv, buf, size, rank, tag);
  } else {
    mv_send_rdz(mv, buf, size, rank, tag, mv_get_sync());
  }
}
