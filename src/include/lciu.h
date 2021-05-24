#ifndef LCI_LCIU_H
#define LCI_LCIU_H

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define __UNUSED__ __attribute__((unused))

#define STATIC_ASSERT(COND,MSG) typedef char static_assertion_##MSG[(COND)?1:-1]

static inline void LCII_MEM_FENCE()
{
  asm volatile("mfence" ::: "memory");
}

#include "lcm_spinlock.h"
#include "lciu_mem.h"

#if 0
#define LC_SET_REQ_DONE_AND_SIGNAL(t, r)                                 \
  {                                                                      \
    register void* sync = (r)->sync;                                     \
    if (t && sync == NULL)                                               \
      sync = __sync_val_compare_and_swap(&(r)->sync, NULL, LC_REQ_DONE); \
    (r)->type = LC_REQ_DONE;                                             \
    if (t && sync) {                                                     \
      if (t == LC_SYNC_WAKE)                                             \
        lc_sync_signal(sync);                                            \
      else if (t == LC_SYNC_CNTR) {                                      \
        if (__sync_fetch_and_sub(&((lc_cntr*)sync)->count, 1) - 1 == 0)  \
          lc_sync_signal(((lc_cntr*)sync)->sync);                        \
      }                                                                  \
    }                                                                    \
    LCII_MEM_FENCE();                                                      \
  }
#endif

#endif // LCI_LCIU_H
