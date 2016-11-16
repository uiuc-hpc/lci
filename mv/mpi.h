#ifndef MPIV_MPI_H_
#define MPIV_MPI_H_

#include "request.h"

#include "irecv.h"
#include "isend.h"
#include "recv.h"
#include "send.h"

#include "waitall.h"
// #include "coll/collective.h"

void MPIV_Recv(void* buffer, int count, MPI_Datatype datatype, int rank,
               int tag, MPI_Comm, MPI_Status*) {
  int size;
  MPI_Type_size(datatype, &size);
  mv_recv(buffer, size * count, rank, tag);
}

void MPIV_Send(void* buffer, int count, MPI_Datatype datatype, int rank,
               int tag, MPI_Comm) {
  int size;
  MPI_Type_size(datatype, &size);
  mv_send(buffer, size * count, rank, tag);
}

void MPIV_Irecv(void* buffer, int count, MPI_Datatype datatype, int rank,
                int tag, MPI_Comm, MPIV_Request* s) {
  int size;
  MPI_Type_size(datatype, &size);
  mv_irecv(buffer, size * count, rank, tag, s);
}

void MPIV_Isend(const void* buf, int count, MPI_Datatype datatype, int rank,
                int tag, MPI_Comm, MPIV_Request* req) {
  int size;
  MPI_Type_size(datatype, &size);
  mv_isend(buf, size * count, rank, tag, req);
}

void MPIV_Waitall(int count, MPIV_Request* req) {
  mv_waitall(count, req);
}

void MPIV_Barrier(MPI_Comm comm) { MPI_Barrier(comm); }

void MPIV_Comm_rank(MPI_Comm, int *rank) { *rank = MPIV.me; }

double MPIV_Wtime() {
#if 0
  using namespace std::chrono;
  return duration_cast<duration<double> >(
             high_resolution_clock::now().time_since_epoch())
      .count();
#endif
    return MPI_Wtime();
}
#endif
