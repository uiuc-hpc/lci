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

extern int mv_send_start, mv_send_end, mv_recv_start, mv_recv_end;
extern int mv_barrier_start, mv_barrier_end;

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
    mv_worker_stop(&MPIV.w[i]);
  }

  mv_worker_stop_main(&MPIV.w[0]);
}

#endif
