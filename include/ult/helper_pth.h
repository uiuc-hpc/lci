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

extern lch* lc_hdl;
lc_pth_thread* MPIV_spawn(int wid, void* (*func)(void*), void*);

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

#if 1
lc_sync* lc_get_sync()
{
  tlself->count = 1;
  return (lc_sync*)tlself;
}

lc_sync* lc_get_counter(int count)
{
  tlself->count = count;
  return (lc_sync*)tlself;
}

void _thread_wait(lc_sync* sync)
{
  lc_pth_thread* thread = (lc_pth_thread*)sync;
  pthread_mutex_lock(&thread->mutex);
  while (thread->count > 0) pthread_cond_wait(&thread->cond, &thread->mutex);
  pthread_mutex_unlock(&thread->mutex);
}

void _thread_signal(lc_sync* sync)
{
  lc_pth_thread* thread = (lc_pth_thread*)sync;
  pthread_mutex_lock(&thread->mutex);
  thread->count--;
  if (thread->count == 0) pthread_cond_signal(&thread->cond);
  pthread_mutex_unlock(&thread->mutex);
}

void _thread_yield() { sched_yield(); }
#endif

typedef struct lc_pth_thread* lc_thread;
lc_pth_thread* MPIV_spawn(int wid, void* (*func)(void*), void* arg)
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

void MPIV_start_worker(int number)
{
  thread_yield = _thread_yield;
  thread_wait = _thread_wait;
  thread_signal = _thread_signal;

  nworker = number;
  tlself = (lc_pth_thread*)malloc(sizeof(struct lc_pth_thread));
  pthread_mutex_init(&tlself->mutex, 0);
  pthread_cond_init(&tlself->cond, 0);
}

void MPIV_stop_worker()
{
  free(tlself);
}

void MPIV_join(lc_pth_thread* ult)
{
  pthread_join(ult->thread, 0);
  free(ult);
}

#endif
