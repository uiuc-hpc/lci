#include "mv.h"
#include "mv-inl.h"

extern int mv_recv_start, mv_recv_end;

void mv_recv(mv_engine*, void*, size_t, int, int);

void mv_recv(mv_engine* mv, void* buffer, size_t size, int rank, int tag) {
#if USE_MPE
  MPE_Log_event(mv_recv_start, 0, "start_recv");
#endif
  MPIV_Request s(buffer, size, rank, tag);
  s.sync = mv_get_sync();
  s.type = REQ_NULL;

  if ((size_t)size <= SHORT_MSG_SIZE) {
    proto_recv_short(mv, buffer, size, rank, tag, &s);
  } else {
    proto_recv_rndz(mv, buffer, size, rank, tag, &s);
    mv_key key = mv_make_key(rank, tag);
    mv_value value = 0;
    if (mv_hash_insert(mv->tbl, key, &value)) {
      thread_wait(s.sync);
    }
  }
#if USE_MPE
  MPE_Log_event(mv_recv_end, 0, "end_recv");
#endif
}
