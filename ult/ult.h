#ifndef ULT_H_
#define ULT_H_

#include "macro.h"

#define xlikely(x) __builtin_expect((x), 1)
#define xunlikely(x) __builtin_expect((x), 0)

#include <boost/function.hpp>
typedef boost::function<void(intptr_t)> ffunc;

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
typedef fthread* mv_thread;
typedef fworker* mv_worker;
typedef fthread mv_sync;

#define mv_worker_spawn fworker_spawn
#define mv_worker_init fworker_init
#define mv_worker_worker fworker_work
#define mv_worker_sched_thread fworker_sched_thread
#define mv_worker_fini_thread fworker_fini_thread
#define mv_worker_start fworker_start
#define mv_worker_stop forker_stop
#define mv_worker_start_main fworker_start_main
#define mv_worker_stop_main fworker_stop_main
#define mv_worker_id fworker_id

MV_INLINE void mv_join(mv_thread thread) {
  fthread* t = (fthread*) thread;
  t->join();
}

MV_INLINE mv_sync* mv_get_sync() {
  tlself.thread->count = -1;
  return tlself.thread;
}

MV_INLINE mv_sync* mv_get_counter(int count) {
  tlself.thread->count = count;
  return tlself.thread;
}

MV_INLINE void thread_wait(mv_sync* sync) {
  fthread* thread = (fthread*) sync;
  thread->wait();
}

MV_INLINE void thread_signal(mv_sync* sync) {
  fthread* thread = (fthread*) sync;
  // smaller than 0 means no counter, saving abit cycles and data.
  if (thread->count < 0) thread->resume();
  else if(thread->count.fetch_sub(1) - 1 == 0) {
    thread->resume();
  }
}

#endif
#endif

#define ult_yield() tlself.thread->yield() 
#define ult_wait() tlself.thread->wait()

extern __thread int cache_wid;

MV_INLINE int worker_id() {
  if (unlikely(cache_wid == -2)) cache_wid = mv_worker_id(tlself.worker);
  return cache_wid;
}

#endif
