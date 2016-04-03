#ifndef RECV_H_
#define RECV_H_

extern __thread int wid;
extern int mpiv_recv_start, mpiv_recv_end;

void MPIV_Send(const void* buffer, int count, MPI_Datatype, int rank, int tag,
               MPI_Comm);

inline void MPIV_Recv_rndz(void* buffer, int, int rank, int tag,
                           MPIV_Request* s) {
  startt(misc_timing);
  mpiv_packet* p = MPIV.pkpool.get_for_send();
  p->set_header(RECV_READY, MPIV.me, tag);
  p->set_rdz(0, (uintptr_t)s, (uintptr_t)buffer, MPIV.ctx.heap_rkey);
  MPIV.ctx.conn[rank].write_send(p, RNDZ_MSG_SIZE, 0, 0);
  MPIV.pkpool.ret_packet_to(p, wid);
  stopt(misc_timing);
}

inline void MPIV_Recv_short(void* buffer, int size, int rank, int tag,
                            MPIV_Request* s) {
  mpiv_key key = mpiv_make_key(rank, tag);
  mpiv_value value;
  value.request = s;

  // Find if the message has arrived, if not go and make a request.
  startt(tbl_timing);
  auto p = MPIV.tbl.insert(key, value);
  auto in_val = p.first;
  stopt(tbl_timing);

  mpiv_packet* p_ctx = NULL;

  if (xunlikely(value.v == in_val.v)) {
    MPIV_Wait(s);
    stopt(wake_timing);
  } else {
    p_ctx = in_val.packet;
    startt(memcpy_timing);
    memcpy(buffer, p_ctx->buffer(), size);
    stopt(memcpy_timing);

    startt(post_timing);
    MPIV.pkpool.ret_packet_to(p_ctx, wid);
    stopt(post_timing);
  }

  startt(tbl_timing);
  MPIV.tbl.erase(key, p.second);
  stopt(tbl_timing)
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
    MPIV_Recv_short(buffer, size, rank, tag, &s);
  } else {
    MPIV_Recv_rndz(buffer, size, rank, tag, &s);
    MPIV_Wait(&s);
  }
#if USE_MPE
  MPE_Log_event(mpiv_recv_end, 0, "end_recv");
#endif
}

void MPIV_Irecv(void* buffer, int count, MPI_Datatype datatype, int rank,
                int tag, MPI_Comm, MPIV_Request* s) {
  int size = 0;
  MPI_Type_size(datatype, &size);
  size *= count;

  s->buffer = buffer;
  s->size = size;
  s->rank = rank;
  s->tag = tag;
  s->done_ = false;

  if ((size_t)size <= SHORT_MSG_SIZE) {
    MPIV_Recv_short(buffer, size, rank, tag, s);
    s->done_ = true;
  } else {
    MPIV_Recv_rndz(buffer, size, rank, tag, s);
  }
}

#endif
