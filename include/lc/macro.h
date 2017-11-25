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

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define __UNUSED__ __attribute__((unused))

extern int server_deadlock_alert;

#define LC_POOL_GET_OR_RETN(p, x)     \
  if (server_deadlock_alert) return LC_ERR_NOP; \
  lc_packet* x = lc_pool_get_nb((p)); \
  if (x == NULL) return LC_ERR_NOP;   

#define LC_SET_REQ_DONE_AND_SIGNAL(t, r)                                 \
  {                                                                      \
    register void* sync = (r)->sync;                                     \
    if (t && sync == NULL)                                               \
      sync = __sync_val_compare_and_swap(&(r)->sync, NULL, LC_REQ_DONE); \
    (r)->type = LC_REQ_DONE;                                             \
    if (t && sync) {                                                     \
      if (t == LC_SYNC_WAKE) lc_sync_signal(sync);                       \
      else if (t == LC_SYNC_CNTR) {                                      \
        if (__sync_fetch_and_sub(&((lc_cntr*) sync)->count, 1) - 1 == 0) \
          lc_sync_signal(((lc_cntr*)sync)->sync);                        \
      }}                                                                 \
    lc_mem_fence();                                                      \
  }

#endif
