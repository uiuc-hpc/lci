#ifndef LC_MACRO_H_
#define LC_MACRO_H_

#define LC_EXPORT __attribute__((visibility("default")))

#define LC_SAFE(x) {while (!x);}

#define lc_mem_fence()                   \
  {                                      \
    asm volatile("mfence" ::: "memory"); \
  }

#define LC_INLINE static inline __attribute__((always_inline))
#define lc_make_key(r, t) ((((uint64_t)(r) << 32) | (uint64_t)(t)))

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define MIN(x, y) ((x) < (y) ? x : y)
#define MAX(x, y) ((x) > (y) ? x : y)

#define __UNUSED__ __attribute__((unused))

#define LC_POOL_GET_OR_RETN(p, x)     \
  lc_packet* x = lc_pool_get_nb((p)); \
  if (x == NULL) return LC_ERR_NOP;   \
  x->context.runtime = 1;

#define LC_SET_REQ_DONE_AND_SIGNAL(r)                                  \
  {                                                                    \
    void* sync = (r)->sync;                                         \
    if (sync == NULL)                                                  \
      sync = __sync_val_compare_and_swap(&(r)->sync, NULL, (void*)-1); \
    (r)->type = LC_REQ_DONE;                                           \
    if (sync && sync != (void*) -1) LC_SYNC_SIGNAL(sync);              \
  }

#endif
