#ifndef ISEND_H_
#define ISEND_H_

#include <mpi.h>

extern int mpiv_send_start, mpiv_send_end;
void proto_send_rdz(MPIV_Request* s);
void proto_send_short(const void* buffer, int size, int rank, int tag);

void isend(const void* buf, int count, MPI_Datatype datatype, int rank, int tag,
           MPI_Comm, MPIV_Request* req) {
  int size = 0;
  MPI_Type_size(datatype, &size);
  size = count * size;
  if (size <= SHORT_MSG_SIZE) {
    proto_send_short(buf, size, rank, tag);
    req->done_ = true;
  } else {
    new (req) MPIV_Request((void*)buf, size, rank, tag);
  }
}

#endif
