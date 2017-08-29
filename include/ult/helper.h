#ifndef LC_HELPER_H_
#define LC_HELPER_H_

#include "lc.h"

#include "ult/fult/fult.h"
#include "thread.h"

static fworker** all_worker = 0;
static int nworker = 0;
__thread struct tls_t tlself;

extern lch* lc_hdl;

void* thread_get() { return tlself.thread; }
void thread_yield() { fthread_yield(tlself.thread); }
void thread_wait(void* thread, volatile int* mutex)
{
  lc_spin_unlock(mutex);
  fthread_wait(thread);
}

void thread_signal(void* thread) { fthread_resume(thread); }
typedef struct fthread* lc_thread;
typedef struct fworker* lc_worker;

void MPI_Start_worker(int number)
{
  lc_sync_init(thread_get, thread_wait, thread_signal, thread_yield);
  if (nworker == 0) {
    all_worker = (fworker**)malloc(sizeof(struct fworker*) * number);
  }
  nworker = number;
  fworker_create(&all_worker[0]);
  all_worker[0]->id = 0;
  for (int i = 1; i < nworker; i++) {
    fworker_create(&all_worker[i]);
    all_worker[i]->id = i;
    fworker_start(all_worker[i]);
  }
  fworker_start_main(all_worker[0]);
}

fthread* MPI_spawn(int wid, void* (*func)(void*), void* arg)
{
  return fworker_spawn(all_worker[wid % nworker], func, arg, F_STACK_SIZE);
}

void MPI_join(fthread* ult) { fthread_join(ult); }
void MPI_Stop_worker()
{
  fworker_stop_main(all_worker[0]);
  fworker_destroy(all_worker[0]);

  for (int i = 1; i < nworker; i++) {
    fworker_stop(all_worker[i]);
    fworker_destroy(all_worker[i]);
  }
}
#endif
