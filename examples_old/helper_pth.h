#ifndef LC_HELPER_PTH_H
#define LC_HELPER_PTH_H

#include "lc.h"
#include "lc/affinity.h"
#include <pthread.h>

static int nworker = 0;
extern __thread int lc_core_id;

typedef void* (*ffunc)(void*);

typedef struct lc_pth_thread {
  struct {
    ffunc f;
    void* data;
    int wid;
    pthread_t thread;
  } __attribute__((aligned(64)));
  struct {
    volatile int count;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
  } __attribute__((aligned(64)));
} lc_pth_thread;

__thread lc_pth_thread* tlself;

lc_pth_thread* MPI_spawn(int wid, void* (*func)(void*), void*);

static void* pth_wrap(void* arg)
{
  lc_pth_thread* th = (lc_pth_thread*)arg;
  set_me_to(th->wid);
  lc_core_id = th->wid;
  tlself = th;
  th->f(th->data);
  tlself = NULL;
  return 0;
}

void* thread_get() { return tlself; }

void _thread_wait(void* t, volatile int* flag)
{
  lc_pth_thread* thread = (lc_pth_thread*)t;
  pthread_mutex_lock(&thread->mutex);
  while (!*flag)
    pthread_cond_wait(&thread->cond, &thread->mutex);
  pthread_mutex_unlock(&thread->mutex);
}

void _thread_signal(void* t)
{
  lc_pth_thread* thread = (lc_pth_thread*)t;
  pthread_cond_signal(&thread->cond);
}

void _thread_yield() { sched_yield(); }

typedef struct lc_pth_thread* lc_thread;
lc_pth_thread* MPI_spawn(int wid, void* (*func)(void*), void* arg)
{
  lc_pth_thread* t = (lc_pth_thread*)malloc(sizeof(struct lc_pth_thread));
  pthread_mutex_init(&t->mutex, 0);
  pthread_cond_init(&t->cond, 0);
  t->f = func;
  t->data = arg;
  t->wid = wid;

  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, 64 * 1024);

  pthread_create(&t->thread, 0, pth_wrap, t);
  return t;
}

void MPI_Start_worker(int number)
{
  lc_sync_init(thread_get, _thread_wait, _thread_signal, _thread_yield);
  nworker = number;
  tlself = (lc_pth_thread*)malloc(sizeof(struct lc_pth_thread));
  pthread_mutex_init(&tlself->mutex, 0);
  pthread_cond_init(&tlself->cond, 0);
}

void MPI_Stop_worker() { free(tlself); }
void MPI_join(lc_pth_thread* ult)
{
  pthread_join(ult->thread, 0);
  free(ult);
}

#endif
