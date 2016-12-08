#ifndef MV_HELPER_ABT_H
#define MV_HELPER_ABT_H

#include "mv.h"
#include <abt.h>
#include "mv/affinity.h"

static ABT_xstream* xstream;
static ABT_pool* pool;
static int nworker = 0;

typedef void(*ffunc)(intptr_t);

typedef struct mv_abt_thread {
  ffunc f;
  intptr_t data;
  int count;
  ABT_thread thread;
  ABT_mutex mutex;
  ABT_cond cond;
} mv_abt_thread;

__thread mv_abt_thread* tlself;

void main_task(intptr_t);
void mv_main_task(intptr_t arg)
{
  set_me_to(0);
  // user-provided.
  main_task(arg);
}

extern mvh* mv_hdl;
ABT_thread MPIV_spawn(int wid, void (*func)(intptr_t), intptr_t arg);

static void setup(intptr_t i)
{
  set_me_to(i);
}

void MPIV_Start_worker(int number)
{
  ABT_init(0, NULL);
  xstream = malloc(sizeof(ABT_xstream) * number);
  pool = malloc(sizeof(ABT_pool) * number);

  nworker = number;
  ABT_xstream_self(&xstream[0]);
  ABT_xstream_get_main_pools(xstream[0], 1, &pool[0]);

  for (int i = 1; i < nworker; i++) {
    ABT_xstream_create(ABT_SCHED_NULL, &xstream[i]);
    ABT_xstream_get_main_pools(xstream[i], 1, &pool[i]);
    ABT_xstream_start(xstream[i]);
    mv_abt_thread* s = MPIV_spawn(0, setup, (intptr_t) i);
    ABT_thread_join(s);
    free(s);
  }

  mv_abt_thread* main_thread = MPIV_spawn(0, mv_main_task, 0);
  ABT_thread_join(main_thread->thread);
  free(main_thread);

  for (int i = 1; i < nworker; i++) {
    ABT_xstream_free(xstream[i]);
  }
  ABT_xstream_free(xstream[0]);
  free(pool);
  free(xstream);

  MPI_Barrier(MPI_COMM_WORLD);
}

static void abt_wrap(void* arg)
{
  mv_abt_thread* th = (mv_abt_thread*)arg;
  tlself = th;
  th->f(th->data);
  tlself = NULL;
}

ABT_thread MPIV_spawn(int wid, void (*func)(intptr_t), intptr_t arg)
{
  mv_abt_thread *t = malloc(sizeof(struct mv_abt_thread));
  ABT_mutex_create(&t->mutex);
  ABT_cond_create(&t->cond);
  t->f = func;
  t->data = arg;
  ABT_thread_create(pool[wid], abt_wrap, t, ABT_THREAD_ATTR_NULL, &t->thread);
  return t;
}

void MPIV_join(mv_abt_thread* ult)
{
  ABT_thread_join(ult->thread);
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
  mv_abt_thread* thread = (mv_abt_thread*)sync;
  if (thread->count < 0) {
    ABT_cond_wait(thread->cond, thread->mutex);
  } else {
    while (thread->count > 0) {
      ABT_cond_wait(thread->cond, thread->mutex);
    }
  }
}

void thread_signal(mv_sync* sync)
{
  mv_abt_thread* thread = (mv_abt_thread*)sync;
  // smaller than 0 means no counter, saving abit cycles and data.
  if (thread->count < 0 || __sync_sub_and_fetch(&thread->count, 1) == 0) {
    ABT_cond_signal(thread->cond);
  }
}
#endif

typedef struct mv_abt_thread* mv_thread;
typedef struct ABT_xstream mv_worker;

#endif
