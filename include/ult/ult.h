#ifndef ULT_H_
#define ULT_H_

#include "lc/affinity.h"
#include "lc/macro.h"
#include <sched.h>
#include <stdint.h>

struct lc_sync;
typedef struct lc_sync lc_sync;

struct lc_sync* lc_get_sync();
struct lc_sync* lc_get_counter(int count);
void thread_wait(lc_sync* sync);
void thread_signal(lc_sync* sync);
void thread_yield();

extern __thread int lc_core_id;

LC_INLINE int lc_worker_id()
{
  if (unlikely(lc_core_id == -1)) {
    lc_core_id = sched_getcpu() % lc_get_ncores();
    if (lc_core_id == -1) lc_core_id = 0;
  }
  return lc_core_id;
}

#endif
