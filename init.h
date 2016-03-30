#ifndef INIT_H_
#define INIT_H_

#include <sys/mman.h>
#include "profiler.h"
#include "progress.h"

inline void mpiv_post_recv(mpiv_packet* p) {
  startt(post_timing);
  MPIV.server.post_srq(p);
  stopt(post_timing);
}

inline void MPIV_Init(int argc, char** args) {
  setenv("MPICH_ASYNC_PROGRESS", "0", 1);
  setenv("MV2_ASYNC_PROGRESS", "0", 1);
  setenv("MV2_ENABLE_AFFINITY", "0", 1);
  int provided;
  MPI_Init_thread(&argc, &args, MPI_THREAD_MULTIPLE, &provided);
  assert(MPI_THREAD_MULTIPLE == provided);

  MPIV.tbl.init();
  mpiv_progress_init();

#ifdef USE_PAPI
  profiler_init();
#endif

  MPIV.server.init(MPIV.ctx, MPIV.pkpool, MPIV.me, MPIV.size);
  MPIV.server.serve();

  MPI_Barrier(MPI_COMM_WORLD);
}

void main_task(intptr_t arg);

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

inline void MPIV_Init_worker(int nworker, intptr_t arg = 0) {
  if (MPIV.w.size() == 0) {
    MPIV.w = std::move(std::vector<worker>(nworker));
    MPIV.pkpool.init_worker(nworker);
  }
  MPIV.w[0].start_main(mpiv_main_task, arg);
}

template <class... Ts>
inline fult_t MPIV_spawn(int wid, Ts... params) {
  return MPIV.w[wid].spawn(params...);
}

inline void MPIV_join(int wid, fult_t t) { MPIV.w[wid].join(t); }

inline void MPIV_Finalize() {
  MPI_Barrier(MPI_COMM_WORLD);
  MPIV.server.finalize();
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Finalize();
}

#endif
