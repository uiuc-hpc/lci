#ifndef LC_LOCK_H_
#define LC_LOCK_H_

#include "macro.h"

#define LC_SPIN_UNLOCKED 0
#define LC_SPIN_LOCKED 1

LC_INLINE void lc_spin_lock(volatile int* flag)
{
  if (__sync_lock_test_and_set(flag, 1)) {
    while (1) {
      while (*flag) {
        __asm__ __volatile__("pause");
      }
      if (!__sync_val_compare_and_swap(flag, 0, 1)) break;
    }
  }
}

LC_INLINE void lc_spin_unlock(volatile int* flag) { __sync_lock_release(flag); }
#endif
