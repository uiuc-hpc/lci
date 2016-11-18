#ifndef RECV_H_
#define RECV_H_

extern int mv_recv_start, mv_recv_end;

MV_INLINE void proto_recv_rndz(void*, int, int, int, MPIV_Request*);
MV_INLINE void proto_recv_short(void*, int, int, int, MPIV_Request*);
MV_INLINE void mv_recv(void*, size_t, int, int);

MV_INLINE void proto_recv_rndz(void* buffer, int, int rank, int tag,
                            MPIV_Request* s) {
  startt(misc_timing);
  packet* p = mv_pp_alloc(MPIV.pkpool, 0);
  assert(p);
  p->header = {RECV_READY, 0, MPIV.me, tag};
  p->content.rdz = {0, (uintptr_t) s, (uintptr_t) buffer, MPIV.server.heap_rkey()};
  MPIV.server.write_send(rank, p, sizeof(packet_header) + sizeof(mv_rdz), p);
  stopt(misc_timing);
}

MV_INLINE void proto_recv_short(void* buffer, int size, int rank, int tag,
                             MPIV_Request* s) {
  mv_key key = mv_make_key(rank, tag);
  mv_value value = (mv_value) s;

  // Find if the message has arrived, if not go and make a request.
  if (likely(!hash_insert(MPIV.tbl, key, &value))) {
    packet* p_ctx = (packet*) value;
    startt(memcpy_timing);
    memcpy(buffer, p_ctx->content.buffer, size);
    stopt(memcpy_timing);
    startt(post_timing);
    mv_pp_free_to(MPIV.pkpool, p_ctx, worker_id() + 1);
    stopt(post_timing);
  } else {
    thread_wait(s->sync);
  }
}

MV_INLINE void mv_recv(void* buffer, size_t size, int rank, int tag) {
#if USE_MPE
  MPE_Log_event(mv_recv_start, 0, "start_recv");
#endif
  MPIV_Request s(buffer, size, rank, tag);
  s.sync = thread_sync_get();
  s.type = REQ_NULL;

  if ((size_t)size <= SHORT_MSG_SIZE) {
    proto_recv_short(buffer, size, rank, tag, &s);
  } else {
    proto_recv_rndz(buffer, size, rank, tag, &s);
    mv_key key = mv_make_key(rank, tag);
    mv_value value;
    if (hash_insert(MPIV.tbl, key, &value)) {
      thread_wait(s.sync);
    }
  }
#if USE_MPE
  MPE_Log_event(mv_recv_end, 0, "end_recv");
#endif
}

#endif
