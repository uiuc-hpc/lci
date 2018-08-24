#ifndef LC_THREAD_H_
#define LC_THREAD_H_

#include "lc/affinity.h"
#include "lc/macro.h"
#include "lc/lock.h"

#include <sched.h>
#include <stdint.h>

typedef void (*lc_signal_fp)(void*);
typedef void (*lc_wait_fp)(void*, volatile int*);

typedef struct lc_sync_fp {
  lc_wait_fp wait;
  lc_signal_fp signal;
} lc_sync_fp;

extern int lcg_current_id;
extern __thread int lcg_core_id;
extern lc_sync_fp g_sync;

LC_INLINE int lc_worker_id()
{
  if (unlikely(lcg_core_id == -1)) {
    lcg_core_id = sched_getcpu();
    if (lcg_core_id == -1) {
      lcg_core_id = __sync_fetch_and_add(&lcg_current_id, 1);
    }
  }
  return lcg_core_id;
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
