#ifndef MPIV_MPIV_H_
#define MPIV_MPIV_H_

#include "mv-inl.h"
#include "mv.h"
#include "request.h"
#include <mpi.h>

typedef uintptr_t MPIV_Request;

extern mv_engine* mv_hdl;

void MPIV_Recv(void* buffer, int count, MPI_Datatype datatype,
                         int rank, int tag, MPI_Comm, MPI_Status*);

void MPIV_Send(void* buffer, int count, MPI_Datatype datatype,
                         int rank, int tag, MPI_Comm);

void MPIV_Irecv(void* buffer, int count, MPI_Datatype datatype, int rank,
                int tag, MPI_Comm, MPIV_Request* req);

void MPIV_Isend(const void* buf, int count, MPI_Datatype datatype, int rank,
                int tag, MPI_Comm, MPIV_Request* req);

void MPIV_Waitall(int count, MPIV_Request* req, MPI_Status*);

extern uintptr_t MPIV_HEAP;

void MPIV_Init(int* argc, char*** args);

void MPIV_Finalize();

MV_INLINE void* MPIV_Alloc(int size) { void* ptr = (void*) MPIV_HEAP; MPIV_HEAP += size; return ptr;}
MV_INLINE void MPIV_Free(void*) { }
#endif
