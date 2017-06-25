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

extern __thread int lc_core_id;

typedef void (*lc_twait_func_t)(lc_sync*);
typedef void (*lc_tyield_func_t)();
typedef void (*lc_tsignal_func_t)(lc_sync*);

extern lc_tyield_func_t thread_yield;
extern lc_twait_func_t thread_wait;
extern lc_tsignal_func_t thread_signal;

LC_INLINE int lc_worker_id()
{
  if (unlikely(lc_core_id == -1)) {
    lc_core_id = sched_getcpu();
    if (lc_core_id == -1) lc_core_id = 0;
  }
  return lc_core_id;
}

#endif
