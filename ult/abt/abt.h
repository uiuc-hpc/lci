#ifndef MPIV_ABT_H_
#define MPIV_ABT_H_

#include <abt.h>

class abt_worker;
class abt_thread;

class abt_thread final : public ult_base {
 friend class abt_worker;
 public:
  abt_thread();
  ~abt_thread() {};

  void yield();
  void wait(bool&);
  void resume(bool&);
  void join();
  void cancel() { ABT_thread_cancel(th_); ABT_thread_resume(th_); }
  int get_worker_id();

  ffunc f;
  intptr_t data;
 private:
  ABT_thread th_;
  ABT_thread_attr attr_;
  ABT_mutex mutex_;
  ABT_cond cond_;
  abt_worker* origin_;
};

class abt_worker final {
 friend class abt_thread;
 public:
  abt_thread* spawn(ffunc f, intptr_t data = 0, size_t stack_size = F_STACK_SIZE);
  void start();
  void stop();
  void start_main(ffunc main_task, intptr_t data);
  void stop_main();
  inline int id() { return id_; }
 private:
  ABT_xstream xstream_;
  ABT_pool pool_;
  int id_;
};

#include "abt_inl.h"

typedef abt_thread* abt_thread_t;

#endif
