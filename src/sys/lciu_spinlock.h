#ifndef LCI_LCIU_SPINLOCK_H
#define LCI_LCIU_SPINLOCK_H

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

typedef atomic_int LCIU_spinlock_t;

static inline void LCIU_spinlock_init(LCIU_spinlock_t* lock)
{
  atomic_init(lock, LCIU_SPIN_UNLOCKED);
  atomic_thread_fence(LCIU_memory_order_seq_cst);
}

static inline void LCIU_spinlock_fina(LCIU_spinlock_t* lock)
{
  atomic_thread_fence(LCIU_memory_order_seq_cst);
  atomic_init(lock, LCIU_SPIN_UNLOCKED);
}

static inline void LCIU_acquire_spinlock(LCIU_spinlock_t* lock)
{
  while (true) {
    while (atomic_load_explicit(lock, LCIU_memory_order_relaxed) ==
           LCIU_SPIN_LOCKED) {
#if defined(__x86_64__)
      asm("pause");
#elif defined(__aarch64__)
      asm("yield");
#endif
    }
    int expected = LCIU_SPIN_UNLOCKED;
    _Bool succeed = atomic_compare_exchange_weak_explicit(
        lock, &expected, LCIU_SPIN_LOCKED, LCIU_memory_order_acquire,
        LCIU_memory_order_relaxed);
    if (succeed) break;
  }
}

// return 1: succeed; return 0: unsucceed
static inline int LCIU_try_acquire_spinlock(LCIU_spinlock_t* lock)
{
  if (atomic_load_explicit(lock, LCIU_memory_order_relaxed) == LCIU_SPIN_LOCKED)
    return false;
  int expected = LCIU_SPIN_UNLOCKED;
  _Bool succeed = atomic_compare_exchange_weak_explicit(
      lock, &expected, LCIU_SPIN_LOCKED, LCIU_memory_order_acquire,
      LCIU_memory_order_relaxed);
  return succeed;
}

static inline void LCIU_release_spinlock(LCIU_spinlock_t* lock)
{
  atomic_store_explicit(lock, LCIU_SPIN_UNLOCKED, LCIU_memory_order_release);
}

#ifdef __cplusplus
}
#endif

#endif  // LCI_LCIU_MISC_H
