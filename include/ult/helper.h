#ifndef LC_HELPER_H_
#define LC_HELPER_H_

#include "lc.h"
#include "ult/fult/fult.h"
#include "ult/ult.h"

static fworker** all_worker = 0;
static int nworker = 0;
__thread struct tls_t tlself;

extern lch* lc_hdl;

void thread_yield() { fthread_yield(tlself.thread); }
void thread_wait(lc_sync* sync)
{
  fthread* thread = (fthread*)sync;
  if (thread->count < 0) {
    fthread_wait(thread);
  } else {
    while (thread->count > 0) {
      fthread_wait(thread);
    }
  }
}

void thread_signal(lc_sync* sync)
{
  fthread* thread = (fthread*)sync;
  // smaller than 0 means no counter, saving abit cycles and data.
  if (thread->count < 0 || __sync_sub_and_fetch(&thread->count, 1) == 0) {
    fthread_resume(thread);
  }
}

lc_sync* lc_get_sync()
{
  tlself.thread->count = -1;
  return (lc_sync*)tlself.thread;
}

lc_sync* lc_get_counter(int count)
{
  tlself.thread->count = count;
  return (lc_sync*)tlself.thread;
}

typedef struct fthread* lc_thread;
typedef struct fworker* lc_worker;

void MPI_Start_worker(int number)
{
  if (nworker == 0) {
    all_worker = (fworker**)malloc(sizeof(struct fworker*) * number);
  }
  nworker = number;
  fworker_init(&all_worker[0]);
  all_worker[0]->id = 0;
  for (int i = 1; i < nworker; i++) {
    fworker_init(&all_worker[i]);
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
