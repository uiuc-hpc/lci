#ifndef MV_PROTO_H_
#define MV_PROTO_H_

MV_INLINE void proto_complete_rndz(mv_engine* mv, packet* p, MPIV_Request* s) {
  p->header = {SEND_WRITE_FIN, 0, mv->me, s->tag};
  p->content.rdz.sreq = (uintptr_t)s;
  mv_server_rma(mv->server, s->rank, s->buffer,
          (void*)p->content.rdz.tgt_addr,
          p->content.rdz.rkey, s->size, (void*)p);
}

MV_INLINE void mv_send_rdz(mv_engine* mv, const void* buffer, int size, int rank, int tag, mv_sync* sync) {
  MPIV_Request s((void*)buffer, size, rank, tag);
  s.sync = sync;
  mv_key key = mv_make_rdz_key(rank, tag);
  mv_value value = (mv_value) &s;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    proto_complete_rndz(mv, (packet*) value, &s);
  }
  key = mv_make_key(rank, (1 << 30) | tag);
  if (mv_hash_insert(mv->tbl, key, &value)) {
    thread_wait(s.sync);
  }
}

MV_INLINE void mv_send_eager(mv_engine* mv, const void* buffer, int size, int rank, int tag) {
  // Get from my pool.
  const int8_t pid = mv_my_worker_id() + 1;
  packet* packet = mv_pp_alloc(mv->pkpool, pid);
  packet->header = {SEND_SHORT, pid, mv->me, tag};
  
  // This is a eager message, we send them immediately and do not yield
  // or create a request for it.
  // Copy the buffer.
  memcpy(packet->content.buffer, buffer, size);

  mv_server_send(mv->server, rank, (void*)packet,
      (size_t) (size + sizeof(packet_header)), (void*) (packet));
}

MV_INLINE void mv_recv_rdz(mv_engine* mv, void* buffer, int size, int rank, int tag, mv_sync* sync) {
  MPIV_Request s(buffer, size, rank, tag);
  s.sync = sync;

  packet* p = mv_pp_alloc(mv->pkpool, 0);
  p->header = {RECV_READY, 0, mv->me, tag};
  p->content.rdz = {0, (uintptr_t) &s, (uintptr_t) buffer, mv_server_heap_rkey(mv->server)};
  mv_server_send(mv->server, rank, p, sizeof(packet_header) + sizeof(mv_rdz), p);

  mv_key key = mv_make_key(rank, tag);
  mv_value value = 0;
  if (mv_hash_insert(mv->tbl, key, &value)) {
    thread_wait(sync);
  }
}

MV_INLINE void mv_recv_eager(mv_engine* mv, void* buffer, int size, int rank, int tag,
                             mv_sync* sync) {
  MPIV_Request s(buffer, size, rank, tag);
  s.sync = sync;

  mv_key key = mv_make_key(rank, tag);
  mv_value value = (mv_value) &s;
  if (mv_hash_insert(mv->tbl, key, &value)) {
    thread_wait(sync);
  } else {
    packet* p_ctx = (packet*) value;
    memcpy(buffer, p_ctx->content.buffer, size);
    mv_pp_free_to(mv->pkpool, p_ctx, mv_my_worker_id());
  }
}

#endif
