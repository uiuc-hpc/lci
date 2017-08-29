#ifndef LC_THREAD_H_
#define LC_THREAD_H_

#include "lc/affinity.h"
#include "lc/macro.h"
#include "lc/lock.h"

#include <sched.h>
#include <stdint.h>

struct lc_sync {
  void* queue;
  volatile int mutex;
  int count;
};

typedef struct lc_sync lc_sync;

typedef void* (*lc_get_fp)();
typedef void (*lc_signal_fp)(void*);
typedef void (*lc_wait_fp)(void*, volatile int*);
typedef void (*lc_yield_fp)();

typedef struct lc_sync_fp {
  lc_get_fp get;
  lc_wait_fp wait;
  lc_yield_fp yield;
  lc_signal_fp signal;
} lc_sync_fp;

extern __thread int lc_core_id;

lc_sync_fp g_sync;

void lc_sync_init(lc_get_fp g, lc_wait_fp w, lc_signal_fp s, lc_yield_fp y);

LC_INLINE int lc_worker_id()
{
  if (unlikely(lc_core_id == -1)) {
    lc_core_id = sched_getcpu();
    if (lc_core_id == -1) lc_core_id = 0;
  }
  return lc_core_id;
}

LC_INLINE void lc_sync_wait(lc_sync* sync, volatile int* flag)
{
  void* thread_ctx = g_sync.get();
  while (!*flag) {
    lc_spin_lock(&sync->mutex);
    if (*flag) {
      lc_spin_unlock(&sync->mutex);
      return;
    }
    sync->queue = thread_ctx;
    g_sync.wait(thread_ctx, &sync->mutex);
  }
}

LC_INLINE void lc_sync_signal(lc_sync* sync)
{
  if (sync->count < 0 || __sync_sub_and_fetch(&sync->count, 1) == 0) {
    lc_spin_lock(&sync->mutex);
    if (!sync->queue) {
      lc_spin_unlock(&sync->mutex);
      return;
    }
    lc_spin_unlock(&sync->mutex);
    g_sync.signal(sync->queue);
  }
}

#endif
