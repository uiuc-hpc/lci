#ifndef MV_HELPER_H_
#define MV_HELPER_H_

#include "mpiv.h"
#include "mv.h"
#include "ult/fult/fult.h"
#include <vector>

static std::vector<fworker*> all_worker;
__thread tls_t tlself;
__thread int worker_id = -2;

int mv_my_worker_id() {
  if (unlikely(worker_id == -2)) worker_id = tlself.worker->id;
  return worker_id;
}

void main_task(intptr_t);
void mv_main_task(intptr_t arg) {
  // user-provided.
  main_task(arg);

  for (size_t i = 1; i < all_worker.size(); i++) {
    fworker_stop(all_worker[i]);
  }

  fworker_stop_main(all_worker[0]);
}

extern mv_engine* mv_hdl;

void MPIV_Start_worker(int number, intptr_t arg = 0) {
  if (all_worker.size() == 0) {
    all_worker = std::move(std::vector<fworker*>(number));
    mv_set_num_worker(mv_hdl, number);
  }
  fworker_init(&all_worker[0]);
  all_worker[0]->id = 0;
  for (size_t i = 1; i < all_worker.size(); i++) {
    fworker_init(&all_worker[i]);
    all_worker[i]->id = i;
    fworker_start(all_worker[i]);
  }
  fworker_start_main(all_worker[0], mv_main_task, arg);
  MPI_Barrier(MPI_COMM_WORLD);
}

fthread* MPIV_spawn(int wid, void (*func)(intptr_t), intptr_t arg) {
  return fworker_spawn(all_worker[wid % all_worker.size()], func, arg);
}

void MPIV_join(fthread* ult) { fthread_join(ult); }

mv_sync* mv_get_sync() {
  tlself.thread->count = -1;
  return (mv_sync*)tlself.thread;
}

mv_sync* mv_get_counter(int count) {
  tlself.thread->count = count;
  return (mv_sync*)tlself.thread;
}

void thread_wait(mv_sync* sync) {
  fthread* thread = (fthread*)sync;
  if (thread->count < 0) {
    fthread_wait(thread);
  } else {
    while (thread->count > 0) fthread_wait(thread);
  }
}

void thread_signal(mv_sync* sync) {
  fthread* thread = (fthread*)sync;
  // smaller than 0 means no counter, saving abit cycles and data.
  if (thread->count < 0 || (__sync_sub_and_fetch(&thread->count, 1) == 0)) {
    fthread_resume(thread);
  }
}

typedef struct fthread* mv_thread;
typedef struct fworker* mv_worker;

#endif
