#ifndef MV_HELPER_ABT_H
#define MV_HELPER_ABT_H

#include "mv.h"
#include "mv/affinity.h"
#include <abt.h>

static ABT_xstream* xstream;
static ABT_pool* pool;
static int nworker = 0;

typedef void (*ffunc)(intptr_t);

typedef struct lc_abt_thread {
  ffunc f;
  intptr_t data;
  int count;
  ABT_thread thread;
  ABT_mutex mutex;
  ABT_cond cond;
  ABT_thread_attr attr;
} lc_abt_thread;

__thread lc_abt_thread* tlself = NULL;

void main_task(intptr_t);

void lc_main_task(intptr_t arg)
{
  set_me_to(0);
  // user-provided.
  main_task(arg);
}

extern lch* lc_hdl;
lc_abt_thread* MPIV_spawn(int wid, void (*func)(intptr_t), intptr_t arg);

static void setup(intptr_t i) { set_me_to(i); }
void MPIV_Start_worker(int number, intptr_t arg)
{
  ABT_init(0, NULL);
  xstream = (ABT_xstream*)malloc(sizeof(ABT_xstream) * number);
  pool = (ABT_pool*)malloc(sizeof(ABT_pool) * number);

  nworker = number;
  ABT_xstream_self(&xstream[0]);
  ABT_xstream_get_main_pools(xstream[0], 1, &pool[0]);
  ABT_xstream_start(xstream[0]);

  for (int i = 1; i < nworker; i++) {
    ABT_xstream_create(ABT_SCHED_NULL, &xstream[i]);
    ABT_xstream_get_main_pools(xstream[i], 1, &pool[i]);
    ABT_xstream_start(xstream[i]);
    lc_abt_thread* s = MPIV_spawn(i, setup, (intptr_t)i);
    ABT_thread_join(s->thread);
    free(s);
  }

  lc_abt_thread* s = MPIV_spawn(0, lc_main_task, arg);
  ABT_thread_join(s->thread);
  free(s);

  for (int i = 1; i < nworker; i++) {
    ABT_xstream_free(&xstream[i]);
  }
  free(pool);
  free(xstream);

  MPI_Barrier(MPI_COMM_WORLD);
}

static void abt_wrap(void* arg)
{
  lc_abt_thread* th = (lc_abt_thread*)arg;
  tlself = th;
  th->f(th->data);
  tlself = NULL;
}

lc_abt_thread* MPIV_spawn(int wid, void (*func)(intptr_t), intptr_t arg)
{
  lc_abt_thread* t = (lc_abt_thread*)malloc(sizeof(struct lc_abt_thread));
  ABT_mutex_create(&t->mutex);
  ABT_cond_create(&t->cond);
  ABT_thread_attr_create(&t->attr);
  t->f = func;
  t->data = arg;
  ABT_thread_attr_set_stacksize(t->attr, 16 * 1024);
  ABT_thread_create(pool[wid], abt_wrap, t, t->attr, &t->thread);
  return t;
}

void MPIV_join(lc_abt_thread* ult)
{
  lc_abt_thread* saved = tlself;
  ABT_thread_join(ult->thread);
  tlself = saved;
  free(ult);
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

void thread_wait(lc_sync* sync)
{
  lc_abt_thread* thread = (lc_abt_thread*)sync;
  lc_abt_thread* saved = tlself;

  ABT_mutex_lock(thread->mutex);
  while (thread->count > 0) {
    ABT_cond_wait(thread->cond, thread->mutex);
  }
  ABT_mutex_unlock(thread->mutex);
  tlself = saved;
}

void thread_signal(lc_sync* sync)
{
  lc_abt_thread* thread = (lc_abt_thread*)sync;
  lc_abt_thread* saved = tlself;
  ABT_mutex_lock(thread->mutex);
  thread->count--;
  if (thread->count == 0) ABT_cond_signal(thread->cond);
  ABT_mutex_unlock(thread->mutex);
  tlself = saved;
}

void thread_yield()
{
  lc_abt_thread* saved = tlself;
  ABT_thread_yield();
  tlself = saved;
}
#endif

typedef lc_abt_thread* lc_thread;
typedef ABT_xstream lc_worker;

#endif
