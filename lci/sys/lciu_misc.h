#ifndef LCI_LCIU_MISC_H
#define LCI_LCIU_MISC_H

#include <assert.h>
#include <time.h>

#define LCIU_MIN(x, y) ((x) < (y) ? (x) : (y))
#define LCIU_MAX(x, y) ((x) > (y) ? (x) : (y))
#define LCIU_MAX_ASSIGN(x, y) x = LCIU_MAX(x, y)

#define __UNUSED__ __attribute__((unused))

#define LCIU_STATIC_ASSERT(COND, MSG) \
  typedef char static_assertion_##MSG[(COND) ? 1 : -1]

#define LCIU_CONCAT3(a, b, c) LCIU_CONCAT_INNER3(a, b, c)
#define LCIU_CONCAT2(a, b) LCIU_CONCAT_INNER2(a, b)
#define LCIU_CONCAT_INNER3(a, b, c) a##b##c
#define LCIU_CONCAT_INNER2(a, b) a##b
#define LCIU_CACHE_PADDING(size) \
  char LCIU_CONCAT2(padding, __LINE__)[LCI_CACHE_LINE - (size) % LCI_CACHE_LINE]

static inline void LCIU_update_average(int64_t* average0, int64_t* count0,
                                       int64_t average1, int64_t count1)
{
  if (*count0 + count1 == 0) {
    return;
  }
  *average0 = (*average0 * *count0 + average1 * count1) / (*count0 + count1);
  *count0 += count1;
}

static inline uint32_t LCIU_set_bits32(uint32_t flag, uint32_t val, int width,
                                       int offset)
{
  assert(width >= 0);
  assert(width <= 32);
  assert(offset >= 0);
  assert(offset <= 32);
  assert(offset + width <= 32);
  const uint32_t val_mask = ((1UL << width) - 1);
  const uint32_t flag_mask = val_mask << offset;
  assert(val <= val_mask);
  flag &= ~flag_mask;                  // clear the bits
  flag |= (val & val_mask) << offset;  // set the bits
  return flag;
}

static inline uint32_t LCIU_get_bits32(uint32_t flag, int width, int offset)
{
  assert(width >= 0);
  assert(width <= 32);
  assert(offset >= 0);
  assert(offset <= 32);
  assert(offset + width <= 32);
  return (flag >> offset) & ((1UL << width) - 1);
}

static inline uint64_t LCIU_set_bits64(uint64_t flag, uint64_t val, int width,
                                       int offset)
{
  assert(width >= 0);
  assert(width <= 64);
  assert(offset >= 0);
  assert(offset <= 64);
  assert(offset + width <= 64);
  const uint64_t val_mask = ((1UL << width) - 1);
  const uint64_t flag_mask = val_mask << offset;
  assert(val <= val_mask);
  flag &= ~flag_mask;                  // clear the bits
  flag |= (val & val_mask) << offset;  // set the bits
  return flag;
}

static inline uint64_t LCIU_get_bits64(uint64_t flag, int width, int offset)
{
  assert(width >= 0);
  assert(width <= 64);
  assert(offset >= 0);
  assert(offset <= 64);
  assert(offset + width <= 64);
  return (flag >> offset) & ((1UL << width) - 1);
}

extern __thread unsigned int LCIU_rand_seed;
static inline int LCIU_rand()
{
  if (LCIU_rand_seed == 0) {
    LCIU_rand_seed = time(NULL) + LCT_get_thread_id() + rand();
  }
  return rand_r(&LCIU_rand_seed);
}

// getenv
static inline int LCIU_getenv_or(char* env, int def)
{
  int ret;
  char* val = getenv(env);
  if (val != NULL) {
    ret = atoi(val);
  } else {
    ret = def;
  }
  LCI_Log(LCI_LOG_INFO, "env", "set %s to be %d\n", env, ret);
  return ret;
}
static inline size_t LCIU_getenv_or_ul(char* env, size_t def)
{
  size_t ret;
  char* val = getenv(env);
  if (val != NULL) {
    LCI_Assert(sscanf(val, "%zu", &ret) == 1, "Unknown value: %lu\n", val);
  } else {
    ret = def;
  }
  LCI_Log(LCI_LOG_INFO, "env", "set %s to be %lu\n", env, ret);
  return ret;
}

static inline void LCIU_spin_for_nsec(double t)
{
  if (t <= 0) return;
  LCT_time_t start = LCT_now();
  while (LCT_time_to_ns(LCT_now() - start) < t) continue;
}

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
    LCII_MEM_FENCE();                                                    \
  }
#endif

#endif  // LCI_LCIU_MISC_H
