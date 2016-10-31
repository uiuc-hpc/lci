#ifndef RECV_H_
#define RECV_H_

extern int mpiv_recv_start, mpiv_recv_end;

void MPIV_Send(const void* buffer, int count, MPI_Datatype, int rank, int tag,
               MPI_Comm);

inline void proto_recv_rndz(void* buffer, int, int rank, int tag,
                           MPIV_Request* s) {
  startt(misc_timing);
  #if 0
  char data[RNDZ_MSG_SIZE];
  Packet* p;
  if (__wid >= 0) p = MPIV.pkpool.get_for_send();
  else p = (Packet*) &data[0];
  #else
  Packet* p = MPIV.pkpool.get_for_send();
  #endif
  p->set_header(RECV_READY, MPIV.me, tag);
  p->set_rdz(0, (uintptr_t)s, (uintptr_t)buffer, MPIV.server.heap_rkey());
  MPIV.server.write_send(rank, p, RNDZ_MSG_SIZE, p);
  // MPIV.pkpool.ret_packet_to(p, mpiv_worker_id());
  stopt(misc_timing);
}

inline void proto_recv_short(void* buffer, int size, int rank, int tag,
                            MPIV_Request* s) {
  mpiv_key key = mpiv_make_key(rank, tag);
  mpiv_value value;
  value.request = s;

  // Find if the message has arrived, if not go and make a request.
  if (!MPIV.tbl.insert(key, value)) {
    Packet* p_ctx = value.packet;
    startt(memcpy_timing);
    memcpy(buffer, p_ctx->buffer(), size);
    stopt(memcpy_timing);

    startt(post_timing);
    MPIV.pkpool.ret_packet_to(p_ctx, worker_id());
    stopt(post_timing);
    s->done_ = true;
  }
}

void MPIV_Recv(void* buffer, int count, MPI_Datatype datatype, int rank,
               int tag, MPI_Comm, MPI_Status*) {
#if USE_MPE
  MPE_Log_event(mpiv_recv_start, 0, "start_recv");
#endif
  int size = 0;
  MPI_Type_size(datatype, &size);
  size *= count;

  MPIV_Request s(buffer, size, rank, tag);

  if ((size_t)size <= SHORT_MSG_SIZE) {
    assert(s.counter == 0);
    proto_recv_short(buffer, size, rank, tag, &s);
  } else {
    proto_recv_rndz(buffer, size, rank, tag, &s);
  }
  MPIV_Wait(&s);
#if USE_MPE
  MPE_Log_event(mpiv_recv_end, 0, "end_recv");
#endif
}

#endif
