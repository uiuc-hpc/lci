#ifndef INIT_H_
#define INIT_H_

#if USE_MPE
#include <mpe.h>
#endif

#ifdef USE_ABT
#include <abt.h>
#endif

#include "profiler.h"
#include "progress.h"
#include <sys/mman.h>

// User provide this TODO(danghvu): HACKXXX
void main_task(intptr_t arg);

namespace mpiv {

int mpiv_send_start, mpiv_send_end, mpiv_recv_start, mpiv_recv_end;
int mpiv_barrier_start, mpiv_barrier_end;

inline void mpiv_post_recv(Packet* p) {
  startt(post_timing);
  MPIV.server.post_recv(p);
  stopt(post_timing);
}

inline void init(int* argc, char*** args) {
  setenv("MPICH_ASYNC_PROGRESS", "0", 1);
  setenv("MV2_ASYNC_PROGRESS", "0", 1);
  setenv("MV2_ENABLE_AFFINITY", "0", 1);
  // setenv("ABT_ENV_SET_AFFINITY", "0", 1);

  int provided;
  MPI_Init_thread(argc, args, MPI_THREAD_MULTIPLE, &provided);
  assert(MPI_THREAD_MULTIPLE == provided);

#ifdef USE_ABT
  ABT_init(*argc, *args);
#endif

#if USE_MPE
  MPE_Init_log();
  mpiv_send_start = MPE_Log_get_event_number();
  mpiv_send_end = MPE_Log_get_event_number();
  mpiv_recv_start = MPE_Log_get_event_number();
  mpiv_recv_end = MPE_Log_get_event_number();
  mpiv_barrier_start = MPE_Log_get_event_number();
  mpiv_barrier_end = MPE_Log_get_event_number();

  MPE_Describe_state(mpiv_send_start, mpiv_send_end, "MPIV_SEND", "red");
  MPE_Describe_state(mpiv_recv_start, mpiv_recv_end, "MPIV_RECV", "blue");
  MPE_Describe_state(mpiv_barrier_start, mpiv_barrier_end, "MPIV_BARRIER",
                     "purple");
#endif

  MPIV.tbl.init();
  mpiv_progress_init();

#ifdef USE_PAPI
  profiler_init();
#endif

#ifndef DISABLE_COMM
  MPIV.server.init(MPIV.pkpool, MPIV.me, MPIV.size);
  MPIV.server.serve();
#endif

  MPI_Barrier(MPI_COMM_WORLD);
}

void mpiv_main_task(intptr_t arg) {
  for (size_t i = 1; i < MPIV.w.size(); i++) {
    MPIV.w[i].start();
  }

  // user-provided.
  main_task(arg);

  for (size_t i = 1; i < MPIV.w.size(); i++) {
    MPIV.w[i].stop();
  }
  MPIV.w[0].stop_main();
}

inline void init_worker(int nworker, intptr_t arg) {
  MPI_Barrier(MPI_COMM_WORLD);
  if (MPIV.w.size() == 0) {
    MPIV.w = std::move(std::vector<worker>(nworker));
    MPIV.pkpool.init_worker(nworker);
  }
  MPIV.w[0].start_main(mpiv_main_task, arg);
  MPI_Barrier(MPI_COMM_WORLD);
}

};  // namespace mpiv

void MPIV_Init(int* argc, char*** args) { mpiv::init(argc, args); }

void MPIV_Init_worker(int nworker, intptr_t arg = 0) {
  mpiv::init_worker(nworker, arg);
}

template <class... Ts>
thread MPIV_spawn(int wid, Ts... params) {
  return mpiv::MPIV.w[wid % mpiv::MPIV.w.size()].spawn(params...);
}

void MPIV_join(thread ult) { ult->join(); }

void MPIV_Finalize() {
#ifndef DISABLE_COMM
  MPI_Barrier(MPI_COMM_WORLD);
  mpiv::MPIV.server.finalize();
  MPI_Barrier(MPI_COMM_WORLD);
#endif

#if USE_MPE
  MPE_Finish_log("mpivlog");
#endif
  MPI_Finalize();
}

#endif
