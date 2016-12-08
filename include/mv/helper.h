#ifndef MV_HELPER_H_
#define MV_HELPER_H_

#include "mv.h"
#include "ult/ult.h"
#include "ult/fult/fult.h"

static fworker** all_worker;
static int nworker;
__thread struct tls_t tlself;

void main_task(intptr_t);
void mv_main_task(intptr_t arg)
{
  // user-provided.
  main_task(arg);

  for (int i = 1; i < nworker; i++) {
    fworker_stop(all_worker[i]);
  }

  fworker_stop_main(all_worker[0]);
}

extern mvh* mv_hdl;

void MPIV_Start_worker(int number)
{
  if (nworker == 0) {
    all_worker = malloc(sizeof(struct fworker*) * number);
  }
  nworker = number;
  fworker_init(&all_worker[0]);
  all_worker[0]->id = 0;
  for (int i = 1; i < nworker; i++) {
    fworker_init(&all_worker[i]);
    all_worker[i]->id = i;
    fworker_start(all_worker[i]);
  }
  fworker_start_main(all_worker[0], mv_main_task, 0);
  MPI_Barrier(MPI_COMM_WORLD);
}

fthread* MPIV_spawn(int wid, void (*func)(intptr_t), intptr_t arg)
{
  return fworker_spawn(all_worker[wid % nworker], func, arg, F_STACK_SIZE);
}

void MPIV_join(fthread* ult) { fthread_join(ult); }

mv_sync* mv_get_sync()
{
  tlself.thread->count = -1;
  return (mv_sync*)tlself.thread;
}

mv_sync* mv_get_counter(int count)
{
  tlself.thread->count = count;
  return (mv_sync*)tlself.thread;
}

void thread_wait(mv_sync* sync)
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

void thread_signal(mv_sync* sync)
{
  fthread* thread = (fthread*)sync;
  // smaller than 0 means no counter, saving abit cycles and data.
  if (thread->count < 0 || __sync_sub_and_fetch(&thread->count, 1) == 0) {
    fthread_resume(thread);
  }
}

typedef struct fthread* mv_thread;
typedef struct fworker* mv_worker;

#endif
