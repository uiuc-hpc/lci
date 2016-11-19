#include "mv.h"
#include "mv-inl.h"

extern int mv_send_start, mv_send_end;
void mv_send(mv_engine* mv, const void*, size_t, int, int);

void mv_send(mv_engine* mv, const void* buffer, size_t size, int rank, int tag) {
#if USE_MPE
  MPE_Log_event(mv_send_start, 0, "start_send");
#endif
  // First we need a packet, as we don't want to register.
  if (size <= SHORT_MSG_SIZE) {
    proto_send_short(mv, buffer, size, rank, tag);
  } else {
    MPIV_Request s((void*)buffer, size, rank, tag);
    s.sync = mv_get_sync();
    proto_send_rdz(mv, &s);
    mv_key key = mv_make_key(s.rank, (1 << 30) | s.tag);
    mv_value value = 0;
    if (mv_hash_insert(mv->tbl, key, &value)) {
      thread_wait(s.sync);
    }
  }
#if USE_MPE
  MPE_Log_event(mv_send_end, 0, "end_send");
#endif
}
