#ifndef SEND_H_
#define SEND_H_

#include <mpi.h>

extern int mpiv_send_start, mpiv_send_end;

void mpiv_complete_rndz(Packet* p, MPIV_Request* s);

inline void MPIV_Send_rdz(MPIV_Request* s) {
  mpiv_key key = mpiv_make_key(s->rank, (1 << 31) | s->tag);
  mpiv_value value;
  value.request = s;
  if (!MPIV.tbl.insert(key, value)) {
    mpiv_complete_rndz(value.packet, s);
  }
}

inline void MPIV_Send_short(const void* buffer, int size, int rank, int tag) {
  Packet* packet = MPIV.pkpool.get_for_send();
  packet->set_header(SEND_SHORT, MPIV.me, tag);
  // This is a short message, we send them immediately and do not yield
  // or create a request for it.
  // Copy the buffer.
  packet->set_bytes(buffer, size);
  // packet->header().sreq = (intptr_t) s;

  MPIV.server.write_send(rank, 
      (void*)packet,
      std::max((size_t) 8, (size_t)size) + sizeof(PacketHeader),
      (void*)(packet));
}

void MPIV_Send(const void* buffer, int count, MPI_Datatype datatype, int rank,
               int tag, MPI_Comm) {
#if USE_MPE
  MPE_Log_event(mpiv_send_start, 0, "start_send");
#endif
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

#endif
