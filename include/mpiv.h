#ifndef MPIV_H_
#define MPIV_H_

#include <mpi.h>

void* mv_malloc(size_t size);
void mv_free(void* ptr);

struct MPIV_Request;
struct thread;

void MPIV_Recv(void* buffer, int count, MPI_Datatype datatype, int rank,
               int tag, MPI_Comm, MPI_Status*);

void MPIV_Send(void* buffer, int count, MPI_Datatype datatype, int rank,
               int tag, MPI_Comm);

void MPIV_Irecv(void* buffer, int count, MPI_Datatype datatype, int rank,
                int tag, MPI_Comm, MPIV_Request* s);

void MPIV_Isend(const void* buf, int count, MPI_Datatype datatype, int rank,
                int tag, MPI_Comm, MPIV_Request* req);

void MPIV_Waitall(int count, MPIV_Request* req, MPI_Status*);

void MPIV_Init(int* argc, char*** args);

void MPIV_Start_worker(int number, intptr_t arg = 0);

template <class... Ts>
thread MPIV_spawn(int wid, Ts... params);

void MPIV_join(thread ult);

void MPIV_Finalize();

#endif
