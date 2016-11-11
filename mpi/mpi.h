#ifndef MPIV_MPI_H_
#define MPIV_MPI_H_

#include "request.h"

namespace mpiv {

#include "irecv.h"
#include "isend.h"
#include "recv.h"
#include "send.h"

#include "waitall.h"
// #include "coll/collective.h"
};  // namespace mpiv.

void MPIV_Recv(void* buffer, int count, MPI_Datatype datatype, int rank,
               int tag, MPI_Comm comm, MPI_Status* status) {
  mpiv::recv(buffer, count, datatype, rank, tag, comm, status);
}

void MPIV_Send(void* buffer, int count, MPI_Datatype datatype, int rank,
               int tag, MPI_Comm comm) {
  mpiv::send(buffer, count, datatype, rank, tag, comm);
}

void MPIV_Irecv(void* buffer, int count, MPI_Datatype datatype, int rank,
                int tag, MPI_Comm comm, mpiv::MPIV_Request* s) {
  mpiv::irecv(buffer, count, datatype, rank, tag, comm, s);
}

void MPIV_Isend(const void* buf, int count, MPI_Datatype datatype, int rank,
                int tag, MPI_Comm comm, mpiv::MPIV_Request* req) {
  mpiv::isend(buf, count, datatype, rank, tag, comm, req);
}

void MPIV_Waitall(int count, mpiv::MPIV_Request* req) {
  mpiv::waitall(count, req);
}

void MPIV_Barrier(MPI_Comm comm) { MPI_Barrier(comm); }

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
