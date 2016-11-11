#ifndef ULT_H_
#define ULT_H_

#define xlikely(x) __builtin_expect((x), 1)
#define xunlikely(x) __builtin_expect((x), 0)

#include <boost/function.hpp>
typedef boost::function<void(intptr_t)> ffunc;

class ult_base {
 public:
  virtual void yield() = 0;
  virtual void wait(bool&) = 0;
  virtual void resume(bool&) = 0;
  virtual void join() = 0;
  virtual void cancel() = 0;
};

#ifdef USE_ABT
#include "abt/abt.h"
using thread = abt_thread_t;
using worker = abt_worker;
#else
#ifdef USE_PTHREAD
#include "pthread/upthread.h"
using thread = pthread_thread_t;
using worker = pthread_worker;
#else
#include "fult/fult.h"
using thread = fthread*;
using worker = fworker;
using thread_sync = fthread;
struct thread_counter {
  fthread* thread;
  std::atomic<int> count;
  thread_counter(int count_) {
    count.store(count_);
    thread = tlself.thread;
  }
};

inline void thread_wait(thread_sync* sync) {
  sync->wait();
}

inline void thread_signal(thread_sync* sync) {
  sync->resume();
}

inline void thread_wait(thread_counter* sync) {
  sync->thread->wait();
}

inline void thread_signal(thread_counter* sync) {
  if (sync->count.fetch_sub(1) - 1 == 0) {
    sync->thread->resume();
  }
}

#endif
#endif

#define ult_yield()        \
  {                        \
    if (tlself.thread != NULL) \
      tlself.thread->yield();  \
    else                   \
      sched_yield();       \
  }

#define ult_wait() \
  { tlself.thread->wait(); }

#define worker_id()  \
  ((tlself.worker != NULL)?(tlself.worker->id()):-1)

#endif
