#ifndef MV_PROTO_H_
#define MV_PROTO_H_

MV_INLINE void proto_complete_rndz(mv_engine* mv, packet* p, MPIV_Request* s);
MV_INLINE void proto_send_rdz(mv_engine* mv, MPIV_Request* s);
MV_INLINE void proto_send_short(mv_engine* mv, const void* buffer, int size, int rank, int tag);
MV_INLINE void proto_recv_rndz(mv_engine*, void*, int, int, int, MPIV_Request*);
MV_INLINE void proto_recv_short(mv_engine*, void*, int, int, int, MPIV_Request*);

MV_INLINE void proto_send_rdz(mv_engine* mv, MPIV_Request* s) {
  mv_key key = mv_make_rdz_key(s->rank, s->tag);
  mv_value value = (mv_value) s;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    proto_complete_rndz(mv, (packet*) value, s);
  }
}

MV_INLINE void proto_send_short(mv_engine* mv, const void* buffer, int size, int rank, int tag) {
  // Get from my pool.
  const int8_t pid = mv_my_worker_id() + 1;
  packet* packet = mv_pp_alloc(mv->pkpool, pid);
  packet->header = {SEND_SHORT, pid, mv->me, tag};
  
  // This is a short message, we send them immediately and do not yield
  // or create a request for it.
  // Copy the buffer.
  memcpy(packet->content.buffer, buffer, size);

  mv_server_send(mv->server, rank, (void*)packet,
      (size_t) (size + sizeof(packet_header)), (void*) (packet));
}

MV_INLINE void proto_recv_rndz(mv_engine* mv, void* buffer, int, int rank, int tag,
                            MPIV_Request* s) {
  startt(misc_timing);
  packet* p = mv_pp_alloc(mv->pkpool, 0);
  assert(p);
  p->header = {RECV_READY, 0, mv->me, tag};
  p->content.rdz = {0, (uintptr_t) s, (uintptr_t) buffer, mv_server_heap_rkey(mv->server)};
  mv_server_send(mv->server, rank, p, sizeof(packet_header) + sizeof(mv_rdz), p);
  stopt(misc_timing);
}

MV_INLINE void proto_recv_short(mv_engine* mv, void* buffer, int size, int rank, int tag,
                             MPIV_Request* s) {
  mv_key key = mv_make_key(rank, tag);
  mv_value value = (mv_value) s;

  // Find if the message has arrived, if not go and make a request.
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    packet* p_ctx = (packet*) value;
    startt(memcpy_timing);
    memcpy(buffer, p_ctx->content.buffer, size);
    stopt(memcpy_timing);
    startt(post_timing);
    mv_pp_free_to(mv->pkpool, p_ctx, mv_my_worker_id());
    stopt(post_timing);
  } else {
    thread_wait(s->sync);
  }
}

MV_INLINE void proto_complete_rndz(mv_engine* mv, packet* p, MPIV_Request* s) {
  p->header = {SEND_WRITE_FIN, 0, mv->me, s->tag};
  p->content.rdz.sreq = (uintptr_t)s;
  mv_server_rma(mv->server, s->rank, s->buffer,
          (void*)p->content.rdz.tgt_addr,
          p->content.rdz.rkey, s->size, (void*)p);
}


#endif
