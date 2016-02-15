#ifndef SEND_H_
#define SEND_H_

#include <mpi.h>

inline void MPIV_Send_rdz(const void* buffer, int size, int rank, int tag, MPIV_Request *s) {
  char data[sizeof(mpiv_rdz) + sizeof(mpiv_packet_header)];
  MPIV_Recv(data, sizeof(mpiv_rdz) + sizeof(mpiv_packet_header), MPI_CHAR,
      rank, 1 << 31 | tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  startt(rdma_timing);
  mpiv_packet* p = (mpiv_packet*) data;
  p->set_header(SEND_READY_FIN, MPIV.me, tag);
  p->set_sreq((uintptr_t) s);
  MPIV.ctx.conn[rank].write_rdma(buffer, MPIV.ctx.heap_lkey,
      (void*)p->rdz_tgt_addr(), p->rdz_rkey(),
      size, p);
  MPIV.ctx.conn[rank].write_send(p, sizeof(mpiv_rdz) + sizeof(mpiv_packet_header), 0, 0);
}

inline void MPIV_Send_short(const void* buffer, int size, int rank, int tag) {
  mpiv_packet* packet = MPIV.pk_mgr.get_packet(SEND_SHORT, MPIV.me, tag);
  // This is a short message, we send them immediately and do not yield
  // or create a request for it.
  // Copy the buffer.
  packet->set_bytes(buffer, size);
  MPIV.ctx.conn[rank].write_send(
      (void*)packet, (size_t)size + sizeof(mpiv_packet_header),
      MPIV.ctx.sbuf_lkey, (void*)packet);
}

void MPIV_Send(const void* buffer, int count, MPI_Datatype datatype, int rank, int tag, MPI_Comm) {
  int size = 0;
  MPI_Type_size(datatype, &size);
  size = count * size;
  // First we need a packet, as we don't want to register.
  if (size <= SHORT) {
    MPIV_Send_short(buffer, size, rank, tag);
  } else {
    MPIV_Request s(0, size, rank, tag);
    MPIV_Send_rdz(buffer, size, rank, tag, &s);
    MPIV_Wait(s);
  }
}

void MPIV_Isend(const void* buf, int count, MPI_Datatype datatype, int rank, int tag, MPI_Comm, MPIV_Request* req) {
   int size = 0;
  MPI_Type_size(datatype, &size);
  size = count * size;
  if (size <= SHORT) {
    MPIV_Send_short(buf, size, rank, tag);
    req->done_ = true;
  } else {
    MPIV_Send_rdz(buf, size, rank, tag, req);
  }
}

#endif
