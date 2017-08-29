#ifndef LC_HELPER_ABT_H
#define LC_HELPER_ABT_H

#include "lc.h"
#include "lc/affinity.h"
#include <abt.h>

static ABT_xstream* xstream;
static ABT_pool* pool;
static int nworker = 0;

typedef void (*ffunc)(intptr_t);

typedef struct lc_abt_thread {
  ffunc f;
  intptr_t data;
  ABT_thread thread;
  ABT_cond cond;
  ABT_mutex mutex;
  ABT_thread_attr attr;
} lc_abt_thread;

__thread lc_abt_thread* tlself = NULL;

extern lch* lc_hdl;
lc_abt_thread* MPI_spawn(int wid, void (*func)(intptr_t), intptr_t arg);

void* thread_get() { return tlself; }
void thread_wait(void* t, volatile int* lock)
{
  lc_abt_thread* thread = (lc_abt_thread*)t;
  lc_abt_thread* saved = tlself;
  ABT_mutex_lock(thread->mutex);
  lc_spin_unlock(lock);
  ABT_cond_wait(thread->cond, thread->mutex);
  ABT_mutex_unlock(thread->mutex);
  tlself = saved;
}

void thread_signal(void* t)
{
  lc_abt_thread* thread = (lc_abt_thread*)t;
  lc_abt_thread* saved = tlself;
  ABT_cond_signal(thread->cond);
  tlself = saved;
}

void thread_yield()
{
  lc_abt_thread* saved = tlself;
  ABT_thread_yield();
  tlself = saved;
}

static void setup(intptr_t i) { set_me_to(i); }
void MPI_Start_worker(int number)
{
  ABT_init(0, NULL);
  lc_sync_init(thread_get, thread_wait, thread_signal, thread_yield);
  xstream = (ABT_xstream*)malloc(sizeof(ABT_xstream) * number);
  pool = (ABT_pool*)malloc(sizeof(ABT_pool) * number);

  nworker = number;
  ABT_xstream_self(&xstream[0]);
  ABT_xstream_get_main_pools(xstream[0], 1, &pool[0]);
  ABT_xstream_start(xstream[0]);

  tlself = (lc_abt_thread*)malloc(sizeof(struct lc_abt_thread));
  ABT_thread_attr_create(&tlself->attr);
  ABT_thread_self(&tlself->thread);
  ABT_cond_create(&tlself->cond);
  ABT_mutex_create(&tlself->mutex);

  for (int i = 1; i < nworker; i++) {
    ABT_xstream_create(ABT_SCHED_NULL, &xstream[i]);
    ABT_xstream_get_main_pools(xstream[i], 1, &pool[i]);
    ABT_xstream_start(xstream[i]);
    lc_abt_thread* s = MPI_spawn(i, setup, (intptr_t)i);
    ABT_thread_join(s->thread);
    free(s);
  }
}

void MPI_Stop_worker() {}
static void abt_wrap(void* arg)
{
  lc_abt_thread* th = (lc_abt_thread*)arg;
  tlself = th;
  th->f(th->data);
  tlself = NULL;
}

lc_abt_thread* MPI_spawn(int wid, void (*func)(intptr_t), intptr_t arg)
{
  lc_abt_thread* t = (lc_abt_thread*)malloc(sizeof(struct lc_abt_thread));
  ABT_thread_attr_create(&t->attr);
  ABT_cond_create(&t->cond);
  ABT_mutex_create(&t->mutex);
  t->f = func;
  t->data = arg;
  ABT_thread_attr_set_stacksize(t->attr, 16 * 1024);
  ABT_thread_create(pool[wid], abt_wrap, t, t->attr, &t->thread);
  return t;
}

void MPI_join(lc_abt_thread* ult)
{
  lc_abt_thread* saved = tlself;
  ABT_thread_join(ult->thread);
  tlself = saved;
  free(ult);
}

typedef lc_abt_thread* lc_thread;
typedef ABT_xstream lc_worker;

#endif
