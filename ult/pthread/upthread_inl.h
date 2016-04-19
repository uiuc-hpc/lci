#ifndef MPIV_PTHREAD_INL_H_
#define MPIV_PTHREAD_INL_H_

#include "affinity.h"

__thread pthread_thread* __fulting = NULL;
__thread int __wid = 0;

pthread_thread::pthread_thread() {
}

void pthread_thread::yield() {
  std::this_thread::yield();
  __fulting = this;
  __wid = this->get_worker_id();
}

void pthread_thread::wait(bool& flag) {
  {
  std::unique_lock<std::mutex> lk(m_);
  cv_.wait(lk, [&flag] { return flag; });
  }
  __fulting = this;
  __wid = this->get_worker_id();
}


void pthread_thread::resume(bool& flag) {
  {
    std::lock_guard<std::mutex> lk(m_);
    flag = true;
  }
  cv_.notify_one();
}

void pthread_thread::join() {
  th_.join();
  __fulting = this;
  __wid = this->get_worker_id();
}

int pthread_thread::get_worker_id() {
  return origin_->id();
}

static void pwrapper(void* arg) {
  pthread_thread* th = (pthread_thread*) arg;
  __fulting = th;
  __wid = th->get_worker_id();
#ifdef USE_AFFI
  affinity::set_me_to(__wid);
#endif
  th->f(th->data);
  __fulting = NULL;
}

pthread_thread* pthread_worker::spawn(ffunc f, intptr_t data, size_t) {
  pthread_thread *th = new pthread_thread();
  th->f = f;
  th->data = data;
  th->origin_ = this;
  th->th_ = std::move(std::thread(pwrapper, th));
  return th;
}

std::atomic<int> pth_nworker;

void pthread_worker::start() {
  id_ = pth_nworker.fetch_add(1);
}

void pthread_worker::stop() {
}

void pthread_worker::start_main(ffunc main_task, intptr_t data) {
  id_ = pth_nworker.fetch_add(1);
#ifdef USE_AFFI
  affinity::set_me_to(id_);
#endif
  pthread_thread* t = spawn(main_task, data);
  t->join();
  delete t;
}

void pthread_worker::stop_main() {
}

#endif
