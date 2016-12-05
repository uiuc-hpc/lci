#ifndef MPIV_PTHREAD_INL_H_
#define MPIV_PTHREAD_INL_H_

#include "affinity.h"

__thread pthread_thread* __fulting = NULL;
__thread int __wid = 0;

pthread_thread::pthread_thread()
{
  pthread_mutex_init(&m_, NULL);
  pthread_cond_init(&cv_, NULL);
}

pthread_thread::~pthread_thread()
{
  pthread_mutex_destroy(&m_);
  pthread_cond_destroy(&cv_);
}

void pthread_thread::yield()
{
  auto saved = __fulting;
  pthread_yield();
  if (saved) {
    __fulting = saved;
  }
}

void pthread_thread::wait(bool& flag)
{
  auto saved = __fulting;
  pthread_mutex_lock(&m_);
  while (!flag) {
    pthread_cond_wait(&cv_, &m_);
  }
  pthread_mutex_unlock(&m_);
  if (saved) {
    __fulting = saved;
  }
}

void pthread_thread::resume(bool& flag)
{
  auto saved = __fulting;
  pthread_mutex_lock(&m_);
  flag = true;
  pthread_mutex_unlock(&m_);
  pthread_cond_signal(&cv_);
  if (saved) {
    __fulting = saved;
  }
}

void pthread_thread::join()
{
  auto saved = __fulting;
  pthread_join(th_, NULL);
  if (saved) {
    __fulting = saved;
  }
  delete this;
}

int pthread_thread::get_worker_id() { return origin_->id(); }
static void* pthread_wrapper(void* arg)
{
  pthread_thread* th = (pthread_thread*)arg;
  __fulting = th;
  __wid = th->get_worker_id();
#ifdef USE_AFFI
  affinity::set_me_to(__wid);
#endif
  th->f(th->data);
  __fulting = NULL;
  return 0;
}

pthread_thread* pthread_worker::spawn(ffunc f, intptr_t data, size_t stack_size)
{
  pthread_thread* th = new pthread_thread();
  th->f = f;
  th->data = data;
  th->origin_ = this;

  pthread_attr_t attrs;
  pthread_attr_init(&attrs);
  if (stack_size > 0) pthread_attr_setstacksize(&attrs, stack_size);

  assert(pthread_create(&(th->th_), &attrs, pthread_wrapper, (void*)th) == 0);
  return th;
}

std::atomic<int> pth_nworker;

void pthread_worker::start() { id_ = pth_nworker.fetch_add(1); }
void pthread_worker::stop() {}
void pthread_worker::start_main(ffunc main_task, intptr_t data)
{
  id_ = pth_nworker.fetch_add(1);
  __wid = id_;
#ifdef USE_AFFI
  affinity::set_me_to(id_);
#endif
  pthread_thread* t = spawn(main_task, data, 0);
  t->join();
}

void pthread_worker::stop_main() {}
#endif
