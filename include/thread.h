#ifndef LC_THREAD_H_
#define LC_THREAD_H_

#include "lc/affinity.h"
#include "lc/macro.h"
#include "lc/lock.h"

#include <sched.h>
#include <stdint.h>

#define LC_SYNC_INITIALIZER {0, -1}

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
extern lc_sync_fp g_sync;

void lc_sync_init(lc_get_fp g, lc_wait_fp w, lc_signal_fp s, lc_yield_fp y);

LC_INLINE int lc_worker_id()
{
  if (unlikely(lc_core_id == -1)) {
    lc_core_id = sched_getcpu();
    if (lc_core_id == -1) lc_core_id = 0;
  }
  return lc_core_id;
}

LC_INLINE void lc_sync_wait(void* sync, volatile int* flag)
{
  g_sync.wait(sync, flag);
}

LC_INLINE void lc_sync_signal(void* sync)
{
  g_sync.signal(sync);
}

#endif
