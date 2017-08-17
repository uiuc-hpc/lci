#ifndef LC_THREAD_H_
#define LC_THREAD_H_

#include "lc/affinity.h"
#include "lc/macro.h"
#include <sched.h>
#include <stdint.h>

struct lc_sync;
typedef struct lc_sync lc_sync;

typedef void (*lc_signal_fp)(lc_sync*);
typedef void (*lc_wait_fp)(lc_sync*);
typedef void (*lc_yield_fp)();

typedef struct lc_sync_fp {
  lc_wait_fp wait;
  lc_yield_fp yield;
  lc_signal_fp signal;
} lc_sync_fp;

extern __thread int lc_core_id;

lc_sync_fp g_sync;

void lc_sync_init(lc_wait_fp w, lc_signal_fp s, lc_yield_fp y);

LC_INLINE int lc_worker_id()
{
  if (unlikely(lc_core_id == -1)) {
    lc_core_id = sched_getcpu();
    if (lc_core_id == -1) lc_core_id = 0;
  }
  return lc_core_id;
}

#endif
