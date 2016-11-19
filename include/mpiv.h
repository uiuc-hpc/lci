#ifndef MPIV_MPIV_H_
#define MPIV_MPIV_H_

#include <mpi.h>
#include "mv.h"
#include "request.h"

extern mv_engine* mv_hdl;

MV_INLINE void MPIV_Recv(void* buffer, int count, MPI_Datatype datatype, int rank,
               int tag, MPI_Comm, MPI_Status*) {
  int size;
  MPI_Type_size(datatype, &size);
  mv_recv(mv_hdl, buffer, size * count, rank, tag);
}

MV_INLINE void MPIV_Send(void* buffer, int count, MPI_Datatype datatype, int rank,
               int tag, MPI_Comm) {
  int size;
  MPI_Type_size(datatype, &size);
  mv_send(mv_hdl, buffer, size * count, rank, tag);
}

MV_INLINE void MPIV_Irecv(void* buffer, int count, MPI_Datatype datatype, int rank,
                int tag, MPI_Comm, MPIV_Request* s) {
  int size;
  MPI_Type_size(datatype, &size);
  mv_irecv(mv_hdl, buffer, size * count, rank, tag, s);
}

MV_INLINE void MPIV_Isend(const void* buf, int count, MPI_Datatype datatype, int rank,
                int tag, MPI_Comm, MPIV_Request* req) {
  int size;
  MPI_Type_size(datatype, &size);
  mv_isend(mv_hdl, buf, size * count, rank, tag, req);
}

MV_INLINE void MPIV_Waitall(int count, MPIV_Request* req, MPI_Status*) {
  mv_waitall(mv_hdl, count, req);
}

MV_INLINE void MPIV_Init(int* argc, char*** args) {
  mv_open(argc, args, &mv_hdl);
}

MV_INLINE void MPIV_Finalize() {
  mv_close(mv_hdl);
}

MV_INLINE void* MPIV_Alloc(int size) {
  return mv_malloc(mv_hdl, (size_t)size);
}

MV_INLINE void MPIV_Free(void* ptr) {
  mv_free(mv_hdl, ptr); 
}

#endif
