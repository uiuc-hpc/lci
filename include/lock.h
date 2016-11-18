#ifndef MV_LOCK_H_
#define MV_LOCK_H_

#include "macro.h"

#define MV_SPIN_UNLOCKED 0
#define MV_SPIN_LOCKED 1

MV_INLINE void mv_spin_lock(volatile int *flag)
{
  while (__sync_lock_test_and_set(flag, 1))
      while (*flag)
          __asm__ volatile("pause": : :"memory");
}

MV_INLINE void mv_spin_unlock(volatile int *flag)
{
  __sync_lock_release(flag);
}

#endif
