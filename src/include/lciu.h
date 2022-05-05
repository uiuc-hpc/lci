#ifndef LCI_LCIU_H
#define LCI_LCIU_H

#include <assert.h>

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define LCIU_MIN(x, y) ((x) < (y) ? (x) : (y))
#define LCIU_MAX(x, y) ((x) > (y) ? (x) : (y))

#define __UNUSED__ __attribute__((unused))

#define STATIC_ASSERT(COND,MSG) typedef char static_assertion_##MSG[(COND)?1:-1]

static inline void LCII_MEM_FENCE()
{
  asm volatile("mfence" ::: "memory");
}

static inline uint32_t LCIU_set_bits32(uint32_t flag, uint32_t val, int width, int offset) {
  assert(width >= 0);
  assert(width <= 32);
  assert(offset >= 0);
  assert(offset <= 32);
  assert(offset + width <= 32);
  const uint32_t val_mask = ((1UL << width) - 1);
  const uint32_t flag_mask = val_mask << offset;
  assert(val <= val_mask);
  flag &= ~flag_mask; // clear the bits
  flag |= (val & val_mask) << offset; // set the bits
  return flag;
}

static inline uint32_t LCIU_get_bits32(uint32_t flag, int width, int offset) {
  assert(width >= 0);
  assert(width <= 32);
  assert(offset >= 0);
  assert(offset <= 32);
  assert(offset + width <= 32);
  return (flag >> offset) & ((1UL << width) - 1);
}

static inline uint64_t LCIU_set_bits64(uint64_t flag, uint64_t val, int width, int offset) {
  assert(width >= 0);
  assert(width <= 64);
  assert(offset >= 0);
  assert(offset <= 64);
  assert(offset + width <= 64);
  const uint64_t val_mask = ((1UL << width) - 1);
  const uint64_t flag_mask = val_mask << offset;
  assert(val <= val_mask);
  flag &= ~flag_mask; // clear the bits
  flag |= (val & val_mask) << offset; // set the bits
  return flag;
}

static inline uint64_t LCIU_get_bits64(uint64_t flag, int width, int offset) {
  assert(width >= 0);
  assert(width <= 64);
  assert(offset >= 0);
  assert(offset <= 64);
  assert(offset + width <= 64);
  return (flag >> offset) & ((1UL << width) - 1);
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
