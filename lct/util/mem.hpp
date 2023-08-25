#ifndef LCT_MEM_HPP
#define LCT_MEM_HPP

#include <cstdlib>
#include <cstring>
#include <cassert>

/* Memory Utility Functions */

static inline void* LCTI_memalign(size_t alignment, size_t size)
{
  void* p_ptr;
  int ret = posix_memalign(&p_ptr, alignment, size);
  assert(ret == 0);
  return p_ptr;
}
static inline void LCTI_free(void* ptr) { free(ptr); }

#ifdef LCT_CONFIG_USE_ALIGNED_ALLOC

static inline void* LCTI_malloc(size_t size)
{
  /* Round up to the smallest multiple of LCI_CACHE_LINE
   * which is greater than or equal to size in order to avoid any
   * false-sharing. */
  size = (size + LCT_CACHE_LINE - 1) & (~(LCT_CACHE_LINE - 1));
  return LCTI_memalign(LCT_CACHE_LINE, size);
}

static inline void* LCTI_calloc(size_t num, size_t size)
{
  void* ptr = LCTI_malloc(num * size);
  memset(ptr, 0, num * size);
  return ptr;
}

static inline void* LCTI_realloc(void* ptr, size_t old_size, size_t new_size)
{
  void* new_ptr = LCTI_malloc(new_size);
  memcpy(new_ptr, ptr, (old_size < new_size) ? old_size : new_size);
  LCTI_free(ptr);
  return new_ptr;
}

#else /* LCT_CONFIG_USE_ALIGNED_ALLOC */

static inline void* LCTI_malloc(size_t size) { return malloc(size); }

static inline void* LCTI_calloc(size_t num, size_t size)
{
  return calloc(num, size);
}

static inline void* LCTI_realloc(void* ptr, size_t old_size, size_t new_size)
{
  (void)old_size;
  return realloc(ptr, new_size);
}

#endif /* !LCT_CONFIG_USE_ALIGNED_ALLOC */
#endif /* LCT_MEM_HPP */
