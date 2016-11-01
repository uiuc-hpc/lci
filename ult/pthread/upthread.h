#ifndef MPIV_PTHREAD_H_
#define MPIV_PTHREAD_H_

#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

#include <pthread.h>

class pthread_thread;
class pthread_worker;

class pthread_thread final : public ult_base {
  friend class pthread_worker;

 public:
  pthread_thread();
  ~pthread_thread();

  void yield();
  void wait(bool&);
  void resume(bool&);
  void cancel() { pthread_cancel(th_); }
  void join();
  int get_worker_id();

  ffunc f;
  intptr_t data;

 private:
  pthread_worker* origin_;
  pthread_t th_;
  pthread_mutex_t m_;
  pthread_cond_t cv_;
};

class pthread_worker final {
  friend class pthread_thread;

 public:
  pthread_thread* spawn(ffunc f, intptr_t data = 0,
                        size_t stack_size = F_STACK_SIZE);
  void start();
  void stop();
  void start_main(ffunc main_task, intptr_t data);
  void stop_main();
  inline int id() { return id_; }

 private:
  int id_;
};

#include "upthread_inl.h"

typedef pthread_thread* pthread_thread_t;

#endif
