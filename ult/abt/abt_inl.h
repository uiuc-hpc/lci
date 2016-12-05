#ifndef MPIV_ABT_INL_H_
#define MPIV_ABT_INL_H_

#include "affinity.h"

__thread abt_thread* __fulting = NULL;
__thread int __wid = 0;

abt_thread::abt_thread()
{
  ABT_mutex_create(&mutex_);
  ABT_cond_create(&cond_);
}

void abt_thread::yield()
{
  abt_thread* saved = __fulting;
  ABT_thread_yield();
  if (saved) __fulting = saved;
}

void abt_thread::wait(bool& flag)
{
  abt_thread* saved = __fulting;
  ABT_mutex_lock(mutex_);
  while (!flag) {
    ABT_cond_wait(cond_, mutex_);
  }
  ABT_mutex_unlock(mutex_);
  if (saved) {
    __fulting = saved;
  }
}

void abt_thread::resume(bool& flag)
{
  abt_thread* saved = __fulting;
  ABT_mutex_lock(mutex_);
  flag = true;
  ABT_mutex_unlock(mutex_);
  ABT_cond_signal(cond_);
  if (saved) {
    __fulting = saved;
  }
}

void abt_thread::join()
{
  abt_thread* saved = __fulting;
  ABT_thread_join(th_);
  if (saved) {
    __fulting = saved;
  }
  delete this;
}

int abt_thread::get_worker_id() { return origin_->id(); }
static void abt_fwrapper(void* arg)
{
  abt_thread* th = (abt_thread*)arg;
  __fulting = th;
  th->f(th->data);
  __fulting = NULL;
}

abt_thread* abt_worker::spawn(ffunc f, intptr_t data, size_t stack_size)
{
  abt_thread* th = new abt_thread();
  th->f = f;
  th->data = data;
  th->origin_ = this;
  ABT_thread_attr_create(&(th->attr_));
  ABT_thread_attr_set_stacksize(th->attr_, stack_size);
  ABT_thread_create(pool_, abt_fwrapper, (void*)th, th->attr_, &(th->th_));
  return th;
}

std::atomic<int> abt_nworker;

static void abt_start_up(void* arg)
{
  long id = (long)arg;
  __wid = id;
#ifdef USE_AFFI
  affinity::set_me_to(id);
#endif
}

void abt_worker::start()
{
  id_ = abt_nworker.fetch_add(1);
  ABT_xstream_create(ABT_SCHED_NULL, &xstream_);
  ABT_xstream_get_main_pools(xstream_, 1, &pool_);
  ABT_xstream_start(xstream_);

  ABT_thread start_up;
  ABT_thread_create(pool_, abt_start_up, (void*)(long)id_, ABT_THREAD_ATTR_NULL,
                    &start_up);
  ABT_thread_join(start_up);
}

void abt_worker::stop()
{
  ABT_xstream_join(xstream_);
  ABT_xstream_free(&xstream_);
}

void abt_worker::start_main(ffunc main_task, intptr_t data)
{
  id_ = abt_nworker.fetch_add(1);
  __wid = id_;
#ifdef USE_AFFI
  affinity::set_me_to(id_);
#endif
  ABT_xstream_self(&xstream_);
  ABT_xstream_get_main_pools(xstream_, 1, &pool_);
  auto t = spawn(main_task, data, MAIN_STACK_SIZE);
  t->join();
}

void abt_worker::stop_main() {}
#endif
