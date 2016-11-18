#ifndef SEND_H_
#define SEND_H_

#include <mpi.h>
#include "macro.h"

extern int mv_send_start, mv_send_end;

MV_INLINE void mv_complete_rndz(packet* p, MPIV_Request* s);
MV_INLINE void proto_send_rdz(MPIV_Request* s);
MV_INLINE void proto_send_short(const void* buffer, int size, int rank, int tag);
MV_INLINE void mv_send(const void*, size_t, int, int);

MV_INLINE void proto_send_rdz(MPIV_Request* s) {
  mv_key key = mv_make_key(s->rank, (1 << 31) | s->tag);
  mv_value value = (mv_value) s;
  if (!hash_insert(MPIV.tbl, key, &value)) {
    mv_complete_rndz((packet*) value, s);
  }
}

MV_INLINE void proto_send_short(const void* buffer, int size, int rank, int tag) {
  // Get from my pool.
  const int pid = worker_id() + 1;
  packet* packet = mv_pp_alloc(MPIV.pkpool, pid);
  packet->header = {SEND_SHORT, pid, MPIV.me, tag};
  
  // This is a short message, we send them immediately and do not yield
  // or create a request for it.
  // Copy the buffer.
  memcpy(packet->content.buffer, buffer, size);

  MPIV.server.write_send(
      rank, (void*)packet,
      (size_t) (size + sizeof(packet_header)),
      (void*) (packet));
}

MV_INLINE void mv_send(const void* buffer, size_t size, int rank, int tag) {
#if USE_MPE
  MPE_Log_event(mv_send_start, 0, "start_send");
#endif
  // First we need a packet, as we don't want to register.
  if (size <= SHORT_MSG_SIZE) {
    proto_send_short(buffer, size, rank, tag);
  } else {
    MPIV_Request s((void*)buffer, size, rank, tag);
    s.sync = thread_sync_get();
    proto_send_rdz(&s);
    mv_key key = mv_make_key(s.rank, (1 << 30) | s.tag);
    mv_value value;
    if (hash_insert(MPIV.tbl, key, &value)) {
      thread_wait(s.sync);
    }
  }
// MPIV.total_send--;
#if USE_MPE
  MPE_Log_event(mv_send_end, 0, "end_send");
#endif
}

#endif
