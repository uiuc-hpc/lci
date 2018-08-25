#ifndef LC_THREAD_H_
#define LC_THREAD_H_

#include "lc/affinity.h"
#include "lc/macro.h"
#include "lc/lock.h"

#include <sched.h>
#include <stdint.h>

extern int lcg_current_id;
extern __thread int lcg_core_id;

typedef volatile int lc_sync;

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

LC_INLINE void lc_sync_wait(lc_sync* sync)
{
  while (!*sync)
    ;
}

LC_INLINE void lc_sync_reset(lc_sync* sync)
{
  *sync = 0;
  lc_mem_fence();
}

LC_INLINE void lc_sync_signal(lc_sync* sync)
{
  *sync = 1;
  lc_mem_fence();
}

#endif
