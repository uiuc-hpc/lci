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

inline thread_sync* thread_sync_get(int count = -1) {
  tlself.thread->count = count;
  return tlself.thread;
}

inline void thread_wait(thread_sync*) {
  tlself.thread->wait();
}

inline void thread_signal(thread_sync* sync) {
  fthread* thread = sync;
  // smaller than 0 means no counter, saving abit cycles and data.
  if (thread->count < 0 || thread->count.fetch_sub(1) - 1 == 0) {
    thread->resume();
  }
}

#endif
#endif

inline void ult_yield()
{                        
  if (tlself.thread != NULL) 
    tlself.thread->yield();  
  else                   
    sched_yield();       
}

inline void ult_wait() 
{ tlself.thread->wait(); }

inline int worker_id() {
  return ((tlself.worker != NULL)?(tlself.worker->id()):-1);
}

#endif
