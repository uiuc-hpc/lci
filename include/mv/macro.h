#ifndef MV_MACRO_H_
#define MV_MACRO_H_

#define MV_INLINE static inline __attribute__((always_inline))
#define mv_make_key(r, t) ((((uint64_t)(r) << 32) | (uint64_t)(t)))
#define mv_make_rdz_key(x, y) mv_make_key(x, ((1 << 31) | y));
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define MIN(x, y) ((x)<(y)?x:y)
#define MAX(x, y) ((x)>(y)?x:y)

#define __UNUSED__ __attribute__((unused))


#endif
