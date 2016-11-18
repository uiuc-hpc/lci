#ifndef MPIV_MPIV_H_
#define MPIV_MPIV_H_

#include "mv.h"
// #include "coll/collective.h"

__thread tls_t tlself;
__thread int cache_wid = -2;

int mv_send_start, mv_send_end, mv_recv_start, mv_recv_end;
int mv_barrier_start, mv_barrier_end;

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

void MPIV_Waitall(int count, MPIV_Request* req, MPI_Status*) {
  mv_waitall(count, req);
}

void MPIV_Init(int* argc, char*** args) {
    mv_init(argc, args);
}

void MPIV_Start_worker(int number, intptr_t arg = 0) {
  if (MPIV.w.size() == 0) {
    MPIV.w = std::move(std::vector<mv_worker>(number));
    mv_pp_ext(MPIV.pkpool, number);
  }

  mv_worker_init(&MPIV.w[0]);
  for (size_t i = 1; i < MPIV.w.size(); i++) {
    mv_worker_init(&MPIV.w[i]);
    mv_worker_start(MPIV.w[i]);
  }
  mv_worker_start_main(MPIV.w[0], mv_main_task, arg);
  MPI_Barrier(MPI_COMM_WORLD);
}

mv_thread MPIV_spawn(int wid, void (*func)(intptr_t), intptr_t arg) {
  return mv_worker_spawn(MPIV.w[wid % MPIV.w.size()], func, arg);
}

void MPIV_join(mv_thread ult) { mv_join(ult); }

void MPIV_Finalize() {
#ifndef DISABLE_COMM
  MPI_Barrier(MPI_COMM_WORLD);
  MPIV.server.finalize();
  mv_pp_destroy(MPIV.pkpool);
  MPI_Barrier(MPI_COMM_WORLD);
#endif

#if USE_MPE
  MPE_Finish_log("mpivlog");
#endif
  MPI_Finalize();
}

#endif
