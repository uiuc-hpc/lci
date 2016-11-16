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

int mv_send_start, mv_send_end, mv_recv_start, mv_recv_end;
int mv_barrier_start, mv_barrier_end;

inline void mv_init(int* argc, char*** args) {
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
  mv_send_start = MPE_Log_get_event_number();
  mv_send_end = MPE_Log_get_event_number();
  mv_recv_start = MPE_Log_get_event_number();
  mv_recv_end = MPE_Log_get_event_number();
  mv_barrier_start = MPE_Log_get_event_number();
  mv_barrier_end = MPE_Log_get_event_number();

  MPE_Describe_state(mv_send_start, mv_send_end, "MPIV_SEND", "red");
  MPE_Describe_state(mv_recv_start, mv_recv_end, "MPIV_RECV", "blue");
  MPE_Describe_state(mv_barrier_start, mv_barrier_end, "MPIV_BARRIER",
                     "purple");
#endif

  hash_init(&MPIV.tbl);
  mv_progress_init();

#ifdef USE_PAPI
  profiler_init();
#endif

  mv_pp_init(&MPIV.pkpool);

#ifndef DISABLE_COMM
  MPIV.server.init(MPIV.pkpool, MPIV.me, MPIV.size);
  MPIV.server.serve();
#endif
  MPI_Barrier(MPI_COMM_WORLD);
}

void mv_main_task(intptr_t arg) {
  // user-provided.
  main_task(arg);

  for (size_t i = 1; i < MPIV.w.size(); i++) {
    MPIV.w[i].stop();
  }

  MPIV.w[0].stop_main();
}

void MPIV_Init(int* argc, char*** args) {
    mv_init(argc, args);
}

void MPIV_Start_worker(int number, intptr_t arg = 0) {
  if (MPIV.w.size() == 0) {
    MPIV.w = std::move(std::vector<worker>(number));
    mv_pp_ext(MPIV.pkpool, number);
  }

  for (size_t i = 1; i < MPIV.w.size(); i++) {
    MPIV.w[i].start();
  }

  MPIV.w[0].start_main(mv_main_task, arg);
  MPI_Barrier(MPI_COMM_WORLD);
}

template <class... Ts>
thread MPIV_spawn(int wid, Ts... params) {
  return MPIV.w[wid % MPIV.w.size()].spawn(params...);
}

void MPIV_join(thread ult) { ult->join(); }

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
