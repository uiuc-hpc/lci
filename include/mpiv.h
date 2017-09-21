#ifndef LC_MPI_H_
#define LC_MPI_H_

#include "lc.h"
/**
* @defgroup mpi MPI-like API
*	@{
*/
typedef uintptr_t MPI_Request;
typedef int MPI_Datatype;
typedef lch* MPI_Comm;
typedef void* MPI_Status;
#define MPI_REQUEST_NULL 0
#define MPI_STATUS_IGNORE 0

extern lch* lc_hdl;

#define MPI_BYTE 1
#define MPI_CHAR 1
#define MPI_INT 4
#define MPI_COMM_WORLD lc_hdl

LC_INLINE
void MPI_Comm_rank(MPI_Comm comm, int* rank) { *rank = lc_id(comm); }

LC_INLINE
void MPI_Comm_size(MPI_Comm comm, int* size) { *size = lc_size(comm); }

LC_EXPORT
void MPI_Recv(void* buffer, int count, MPI_Datatype datatype, int rank, int tag,
              MPI_Comm, MPI_Status*);

LC_EXPORT
void MPI_Send(void* buffer, int count, MPI_Datatype datatype, int rank, int tag,
              MPI_Comm);

LC_EXPORT
void MPI_Ssend(void* buffer, int count, MPI_Datatype datatype, int rank,
               int tag, MPI_Comm);

LC_EXPORT
void MPI_Irecv(void* buffer, int count, MPI_Datatype datatype, int rank,
               int tag, MPI_Comm, MPI_Request* req);

LC_EXPORT
void MPI_Isend(const void* buf, int count, MPI_Datatype datatype, int rank,
               int tag, MPI_Comm, MPI_Request* req);

LC_EXPORT
void MPI_Waitall(int count, MPI_Request* req, MPI_Status*);

LC_EXPORT
void MPI_Init(int* argc, char*** args);

LC_EXPORT
void MPI_Finalize();

LC_EXPORT
void* MPI_Alloc(size_t size);

LC_EXPORT
void MPI_Free(void*);

LC_EXPORT
void MPI_Start_worker(int number);

LC_EXPORT
void MPI_Stop_worker();

/**@}*/

#endif
