#ifndef MV_HELPER_PTH_H
#define MV_HELPER_PTH_H

#include "mv.h"
#include <pthread.h>
#include "mv/affinity.h"

static int nworker = 0;

typedef void(*ffunc)(intptr_t);

typedef struct mv_pth_thread {
  ffunc f;
  intptr_t data;
  int count;
  int wid;
  pthread_t thread;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
} mv_pth_thread;

__thread mv_pth_thread* tlself;

void main_task(intptr_t);
void mv_main_task(intptr_t arg)
{
  set_me_to(0);
  // user-provided.
  main_task(arg);
}

extern mvh* mv_hdl;
mv_pth_thread* MPIV_spawn(int wid, void (*func)(intptr_t), intptr_t arg);

void MPIV_Start_worker(int number)
{
  nworker = number;
  mv_pth_thread* main_thread = MPIV_spawn(0, mv_main_task, 0);
  pthread_join(main_thread->thread, 0);
  free(main_thread);
  MPI_Barrier(MPI_COMM_WORLD);
}

static void* pth_wrap(void* arg)
{
  mv_pth_thread* th = (mv_pth_thread*)arg;
  set_me_to(th->wid);
  tlself = th;
  th->f(th->data);
  tlself = NULL;
  return 0;
}

mv_pth_thread* MPIV_spawn(int wid, void (*func)(intptr_t), intptr_t arg)
{
  mv_pth_thread *t = malloc(sizeof(struct mv_pth_thread));
  pthread_mutex_init(&t->mutex, 0);
  pthread_cond_init(&t->cond, 0);
  t->f = func;
  t->data = arg;
  t->wid = wid;
  pthread_create(&t->thread, 0, pth_wrap, t);
  return t;
}

void MPIV_join(mv_pth_thread* ult)
{
  pthread_join(ult->thread, 0);
  free(ult);
}

#if 1
mv_sync* mv_get_sync()
{
  tlself->count = -1;
  return (mv_sync*)tlself;
}

mv_sync* mv_get_counter(int count)
{
  tlself->count = count;
  return (mv_sync*)tlself;
}

void thread_wait(mv_sync* sync)
{
  mv_pth_thread* thread = (mv_pth_thread*)sync;
  pthread_mutex_lock(&thread->mutex);
  if (thread->count < 0) {
    pthread_cond_wait(&thread->cond, &thread->mutex);
  } else {
    while (thread->count > 0) {
      pthread_cond_wait(&thread->cond, &thread->mutex);
    }
  }
  pthread_mutex_unlock(&thread->mutex);
}

void thread_signal(mv_sync* sync)
{
  mv_pth_thread* thread = (mv_pth_thread*)sync;
  // smaller than 0 means no counter, saving abit cycles and data.
  if (thread->count < 0 || __sync_sub_and_fetch(&thread->count, 1) == 0) {
    pthread_cond_signal(&thread->cond);
  }
}
#endif

typedef struct mv_pth_thread* mv_thread;
typedef struct ABT_xstream mv_worker;

#endif
