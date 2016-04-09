#ifndef SEND_H_
#define SEND_H_

#include <mpi.h>

extern __thread int wid;

extern int mpiv_send_start, mpiv_send_end;

void mpiv_complete_rndz(mpiv_packet* p, MPIV_Request* s);

inline void MPIV_Send_rdz(MPIV_Request* s) {
  mpiv_key key = mpiv_make_key(s->rank, (1 << 31) | s->tag);
  mpiv_value value;
  value.request = s;
  auto entry = MPIV.tbl.insert(key, value);
  mpiv_packet* p;
  if (entry.first.v != value.v) {
    p = entry.first.packet;
    mpiv_complete_rndz(p, s);
    MPIV.tbl.erase(key, entry.second);
  }
}

inline void MPIV_Send_short(const void* buffer, int size, int rank, int tag) {
  mpiv_packet* packet = MPIV.pkpool.get_for_send();
  packet->set_header(SEND_SHORT, MPIV.me, tag);
  // This is a short message, we send them immediately and do not yield
  // or create a request for it.
  // Copy the buffer.
  packet->set_bytes(buffer, size);
  MPIV.ctx.conn[rank].write_send(
      (void*)packet,
      ((((size_t)size + sizeof(mpiv_packet_header) + 3) >> 2) << 2),
      MPIV.ctx.sbuf_lkey, (void*)(packet));
}

void MPIV_Send(const void* buffer, int count, MPI_Datatype datatype, int rank,
               int tag, MPI_Comm) {
#if USE_MPE
  MPE_Log_event(mpiv_send_start, 0, "start_send");
#endif
  // if (MPIV.total_send == MAX_SEND) fult_yield();
  // MPIV.total_send++;
  int size = 0;
  MPI_Type_size(datatype, &size);
  size = count * size;
  // First we need a packet, as we don't want to register.
  if (size <= SHORT_MSG_SIZE) {
    MPIV_Send_short(buffer, size, rank, tag);
  } else {
    MPIV_Request s((void*)buffer, size, rank, tag);
    MPIV_Send_rdz(&s);
    MPIV_Wait(&s);
  }
  // MPIV.total_send--;
#if USE_MPE
  MPE_Log_event(mpiv_send_end, 0, "end_send");
#endif
}

void MPIV_Isend(const void* buf, int count, MPI_Datatype datatype, int rank,
                int tag, MPI_Comm, MPIV_Request* req) {
  int size = 0;
  MPI_Type_size(datatype, &size);
  size = count * size;
  new (req) MPIV_Request(rank, tag);

  if (size <= SHORT_MSG_SIZE) {
    MPIV_Send_short(buf, size, rank, tag);
    req->done_ = true;
  } else {
    req->buffer = (void*)buf;
    req->size = size;
    req->rank = rank;
    req->tag = tag;
    MPIV_Send_rdz(req);
  }
}

#endif
