#ifndef LC_THREAD_H_
#define LC_THREAD_H_

#include "lc/affinity.h"
#include "lc/macro.h"
#include "lc/lock.h"

#include <sched.h>
#include <stdint.h>

extern __thread int lc_core_id;

LC_INLINE int lc_worker_id()
{
  if (unlikely(lc_core_id == -1)) {
    lc_core_id = sched_getcpu();
    if (lc_core_id == -1) lc_core_id = 0;
  }
  return lc_core_id;
}

#ifndef LC_SYNC_WAIT
#define LC_SYNC_WAIT(sync, flag) { while (!flag) ; }
#endif

#ifndef LC_SYNC_SIGNAL
#define LC_SYNC_SIGNAL(sync) { }
#endif

#endif
