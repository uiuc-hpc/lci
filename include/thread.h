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

LC_INLINE void lc_wait(void* ce)
{
  lc_sync* sync = (lc_sync*) ce;
  while (!*sync)
    ;
}

LC_INLINE void lc_reset(void* ce)
{
  lc_sync* sync = (lc_sync*) ce;
  *sync = 0;
}

LC_INLINE void lc_signal(void* ce)
{
  lc_sync* sync = (lc_sync*) ce;
  *sync = 1;
}

#endif
