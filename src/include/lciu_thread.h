#ifndef LCI_LCIU_THREAD_H
#define LCI_LCIU_THREAD_H

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

/*
 * We would like to hide these two global variable,
 * but we cannot do it easily, because:
 * - we want to make LCIU_thread_id an inline function
 */
extern int LCIU_nthreads;
extern __thread int LCIU_thread_id;

/* thread id */
static inline int LCIU_get_thread_id()
{
  if (unlikely(LCIU_thread_id == -1)) {
    LCIU_thread_id = sched_getcpu();
    if (LCIU_thread_id == -1) {
      LCIU_thread_id = __sync_fetch_and_add(&LCIU_nthreads, 1);
    }
  }
  return LCIU_thread_id;
}

/* lock */
#define LCIU_SPIN_UNLOCKED 0
#define LCIU_SPIN_LOCKED 1

typedef volatile int LCIU_mutex_t;

static inline void LCIU_acquire_spinlock(LCIU_mutex_t* flag)
{
  if (__sync_lock_test_and_set(flag, LCIU_SPIN_LOCKED)) {
    while (1) {
      while (*flag) {
        asm("pause");
      }
      if (!__sync_val_compare_and_swap(flag, LCIU_SPIN_UNLOCKED, LCIU_SPIN_LOCKED)) break;
    }
  }
}

static inline void LCIU_release_spinlock(LCIU_mutex_t* flag)
{
  __sync_lock_release(flag);
}

/* affinity */
static inline int LCIU_set_me_to(int core_id)
{
#ifdef USE_AFFI
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);

#ifdef AFF_DEBUG
  fprintf(stderr, "[USE_AFFI] Setting someone to core # %d\n", core_id);
#endif
  pthread_t current_thread = pthread_self();
  return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
#else
  return 0;
#endif
}

static inline int LCIU_set_me_within(int from, int to)
{
#ifdef USE_AFFI
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  int i;
  for (i = from; i < to; i++) CPU_SET(i, &cpuset);
  // std::cerr << "[USE_AFFI] Setting someone to core #[" << from << " - " << to
  // <<")" << std::endl;
  pthread_t current_thread = pthread_self();
  return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
#else
  return 0;
#endif
}

#ifdef __cplusplus
}
#endif

#endif  // LCI_LCIU_H
