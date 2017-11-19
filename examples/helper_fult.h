#ifndef LC_HELPER_H_
#define LC_HELPER_H_

#include "ult/fult/fult.h"
#include "lc.h"

static fworker** all_worker = 0;
static int nworker = 0;
__thread struct tls_t tlself;

extern lch* lc_hdl;

void* thread_self() { return tlself.thread; }
void thread_yield() { fthread_yield(tlself.thread); }

typedef struct fthread* lc_thread;
typedef struct fworker* lc_worker;

void MPI_Start_worker(int number)
{
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

void MPI_spawn(int wid, void* (*func)(void*), void* arg, fthread_t* thread)
{
  fworker_spawn(all_worker[wid % nworker], func, arg, F_STACK_SIZE, thread);
}

void MPI_join(fthread_t* ult) { fthread_join(ult); }
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
