#ifndef INIT_H_
#define INIT_H_

#include "config.h"

#if USE_MPE
#include <mpe.h>
#endif

#ifdef USE_ABT
#include <abt.h>
#endif

#include "profiler.h"
#include "progress.h"
#include <sys/mman.h>

// HACK
void main_task(intptr_t);

namespace mpiv {

int mpiv_send_start, mpiv_send_end, mpiv_recv_start, mpiv_recv_end;
int mpiv_barrier_start, mpiv_barrier_end;

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
  // user-provided.
  main_task(arg);

  for (size_t i = 1; i < MPIV.w.size(); i++) {
    MPIV.w[i].stop();
  }

  MPIV.w[0].stop_main();
}

};  // namespace mpiv

void MPIV_Init(int* argc, char*** args) {
    mpiv::init(argc, args);
}

void MPIV_Start_worker(int number, intptr_t arg = 0) {
  if (mpiv::MPIV.w.size() == 0) {
    mpiv::MPIV.w = std::move(std::vector<worker>(number));
    mpiv::MPIV.pkpool.init_worker(number);
  }

  for (size_t i = 1; i < mpiv::MPIV.w.size(); i++) {
    mpiv::MPIV.w[i].start();
  }

  mpiv::MPIV.w[0].start_main(mpiv::mpiv_main_task, arg);
  MPI_Barrier(MPI_COMM_WORLD);
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
