#ifndef ULT_H_
#define ULT_H_

#include "mv/affinity.h"
#include "mv/macro.h"
#include <sched.h>
#include <stdint.h>

struct mv_sync;
typedef struct mv_sync mv_sync;

struct mv_sync* mv_get_sync();
struct mv_sync* mv_get_counter(int count);
void thread_wait(mv_sync* sync);
void thread_signal(mv_sync* sync);
void thread_yield();

extern __thread int mv_core_id;

MV_INLINE int mv_worker_id()
{
  if (unlikely(mv_core_id == -1)) {
    mv_core_id = sched_getcpu() % get_ncores();
    if (mv_core_id == -1) mv_core_id = 0;
  }
  return mv_core_id;
}

#endif
