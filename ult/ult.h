#ifndef ULT_H_
#define ULT_H_

#define xlikely(x) __builtin_expect((x), 1)
#define xunlikely(x) __builtin_expect((x), 0)

#include <boost/function.hpp>
typedef boost::function<void(intptr_t)> ffunc;

static const int F_STACK_SIZE = 64 * 1024;
static const int MAIN_STACK_SIZE = 1024 * 1024;

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
using thread = fult_t;
using worker = fworker;
#endif
#endif

#define ult_yield()        \
  {                        \
    if (__fulting != NULL) \
      __fulting->yield();  \
    else                   \
      sched_yield();       \
  }

#define ult_wait() \
  { __fulting->wait(); }


#endif
