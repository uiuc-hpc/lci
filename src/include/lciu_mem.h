#ifndef LCI_LCIU_MEM_H
#define LCI_LCIU_MEM_H

#include <stdlib.h>
#include <assert.h>

/* Memory Utility Functions */

static inline void *LCIU_memalign(size_t alignment, size_t size)
{
    void *p_ptr;
    int ret = posix_memalign(&p_ptr, alignment, size);
    assert(ret == 0);
    return p_ptr;
}
static inline void LCIU_free(void *ptr)
{
    free(ptr);
}

#ifdef LCI_CONFIG_USE_ALIGNED_ALLOC

static inline void *LCIU_malloc(size_t size)
{
    /* Round up to the smallest multiple of LCI_CACHE_LINE
     * which is greater than or equal to size in order to avoid any
     * false-sharing. */
    size = (size + LC_CACHE_LINE - 1) &
           (~(LC_CACHE_LINE - 1));
    return LCIU_memalign(LC_CACHE_LINE, size);
}

static inline void *LCIU_calloc(size_t num, size_t size)
{
    void *ptr = LCIU_malloc(num * size);
    memset(ptr, 0, num * size);
    return ptr;
}

static inline void *LCIU_realloc(void *ptr, size_t old_size, size_t new_size)
{
    void *new_ptr = LCIU_malloc(new_size);
    memcpy(new_ptr, ptr, (old_size < new_size) ? old_size : new_size);
    LCIU_free(ptr);
    return new_ptr;
}

#else /* LCI_CONFIG_USE_ALIGNED_ALLOC */

static inline void *LCIU_malloc(size_t size)
{
    return malloc(size);
}

static inline void *LCIU_calloc(size_t num, size_t size)
{
    return calloc(num, size);
}

static inline void *LCIU_realloc(void *ptr, size_t old_size, size_t new_size)
{
    (void)old_size;
    return realloc(ptr, new_size);
}

#endif /* !LCI_CONFIG_USE_ALIGNED_ALLOC */

#define LCIU_strcpy(d, s) strcpy(d, s)
#define LCIU_strncpy(d, s, n) strncpy(d, s, n)

/* The caller should free the memory returned. */
char *LCIU_get_indent_str(int indent);

int LCIU_get_int_len(size_t num);

#endif /* LCI_LCIU_MEM_H */
