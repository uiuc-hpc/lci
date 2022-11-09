#ifndef LCI_LCM_SPINLOCK_H
#define LCI_LCM_SPINLOCK_H

#include <stdio.h>
#include <sched.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Most of the functions here are likely to be on the critical path, so we
 * would like to make them inline
 */

/* lock */
#define LCIU_SPIN_UNLOCKED 0
#define LCIU_SPIN_LOCKED 1

typedef volatile int LCIU_spinlock_t;

static inline void LCIU_spinlock_init(LCIU_spinlock_t* lock)
{
  *lock = LCIU_SPIN_UNLOCKED;
}

static inline void LCIU_spinlock_fina(LCIU_spinlock_t* lock)
{
  *lock = LCIU_SPIN_UNLOCKED;
}

static inline void LCIU_acquire_spinlock(LCIU_spinlock_t* lock)
{
  if (__sync_lock_test_and_set(lock, LCIU_SPIN_LOCKED)) {
    while (1) {
      while (*lock) {
        asm("pause");
      }
      if (!__sync_val_compare_and_swap(lock, LCIU_SPIN_UNLOCKED,
                                       LCIU_SPIN_LOCKED))
        break;
    }
  }
}

// return 1: succeed; return 0: unsucceed
static inline int LCIU_try_acquire_spinlock(LCIU_spinlock_t* lock)
{
  return (__sync_lock_test_and_set(lock, LCIU_SPIN_LOCKED) ==
          LCIU_SPIN_UNLOCKED);
}

static inline void LCIU_release_spinlock(LCIU_spinlock_t* lock)
{
  __sync_lock_release(lock);
}

#ifdef __cplusplus
}
#endif

#endif  // LCI_LCIU_H
