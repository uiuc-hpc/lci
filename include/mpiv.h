#ifndef LC_MPIV_H_
#define LC_MPIV_H_

#include "lc.h"
#include <mpi.h>

/**
* @defgroup mpi MPI-like API
*	@{
*/
typedef uintptr_t MPIV_Request;
extern lch* lc_hdl;

LC_EXPORT
void MPIV_Recv(void* buffer, int count, MPI_Datatype datatype, int rank,
               int tag, MPI_Comm, MPI_Status*);

LC_EXPORT
void MPIV_Send(void* buffer, int count, MPI_Datatype datatype, int rank,
               int tag, MPI_Comm);

LC_EXPORT
void MPIV_Ssend(void* buffer, int count, MPI_Datatype datatype, int rank,
                int tag, MPI_Comm);

LC_EXPORT
void MPIV_Irecv(void* buffer, int count, MPI_Datatype datatype, int rank,
                int tag, MPI_Comm, MPIV_Request* req);

LC_EXPORT
void MPIV_Isend(const void* buf, int count, MPI_Datatype datatype, int rank,
                int tag, MPI_Comm, MPIV_Request* req);

LC_EXPORT
void MPIV_Waitall(int count, MPIV_Request* req, MPI_Status*);

LC_EXPORT
void MPIV_Init(int* argc, char*** args);

LC_EXPORT
void MPIV_Finalize();

LC_EXPORT
void* MPIV_Alloc(size_t size);

LC_EXPORT
void MPIV_Free(void*);

/**@}*/

#endif
