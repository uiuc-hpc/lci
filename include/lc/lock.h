#ifndef LC_LOCK_H_
#define LC_LOCK_H_

#include "macro.h"

#define LC_SPIN_UNLOCKED 0
#define LC_SPIN_LOCKED 1

LC_INLINE void lc_spin_lock(volatile int* flag)
{
  while (__sync_lock_test_and_set(flag, 1)) while (*flag);
}

LC_INLINE void lc_spin_unlock(volatile int* flag) { __sync_lock_release(flag); }
#endif
