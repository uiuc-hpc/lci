#ifndef LCI_LCM_ARCHIVE_H
#define LCI_LCM_ARCHIVE_H

#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Used to map address to key with less bits.
 */
#define LCM_SUCCESS 1
#define LCM_RETRY 0

#define LCM_ARCHIVE_EMPTY 0

typedef uint64_t LCM_archive_key_t;
typedef uintptr_t LCM_archive_val_t;

struct LCM_archive_entry_t {
  _Atomic LCM_archive_val_t val;  // 8 bytes
  LCIU_CACHE_PADDING(sizeof(_Atomic LCM_archive_val_t));
};

struct LCM_archive_t {
  atomic_uint_fast64_t head;
  LCIU_CACHE_PADDING(sizeof(atomic_uint_fast64_t));
  struct LCM_archive_entry_t* ptr;
  int nbits;
};
typedef struct LCM_archive_t LCM_archive_t;

static int LCM_archive_init(LCM_archive_t* archive, int nbits);
static int LCM_archive_fini(LCM_archive_t* archive);

static inline int LCM_archive_put(LCM_archive_t* archive,
                                  LCM_archive_val_t value,
                                  LCM_archive_key_t* key);
static inline LCM_archive_val_t LCM_archive_get(LCM_archive_t* archive,
                                                LCM_archive_key_t key);
static inline LCM_archive_val_t LCM_archive_remove(LCM_archive_t* archive,
                                                   LCM_archive_key_t key);

#ifdef __cplusplus
}
#endif

static int LCM_archive_init(LCM_archive_t* archive, int nbits)
{
  assert(sizeof(struct LCM_archive_entry_t) == LCI_CACHE_LINE);
  size_t cap = 1 << nbits;
  int ret = posix_memalign((void**)&archive->ptr, LCI_CACHE_LINE,
                           cap * sizeof(struct LCM_archive_entry_t));
  LCM_Assert(ret == 0, "Memory allocation failed!\n");
  atomic_init(&archive->head, 0);
  archive->nbits = nbits;

  // Initialize all with EMPTY and clear lock.
  for (size_t i = 0; i < cap; i++) {
    atomic_init(&archive->ptr[i].val, LCM_ARCHIVE_EMPTY);
  }
  atomic_thread_fence(LCIU_memory_order_seq_cst);
  return LCM_SUCCESS;
}

static int LCM_archive_fini(LCM_archive_t* archive)
{
  atomic_thread_fence(LCIU_memory_order_seq_cst);
  free((void*)archive->ptr);
  archive->ptr = NULL;
  archive->nbits = 0;
  atomic_init(&archive->head, 0);
  return LCM_SUCCESS;
}

static inline int LCM_archive_put(LCM_archive_t* archive,
                                  LCM_archive_val_t value,
                                  LCM_archive_key_t* key_ptr)
{
  uint_fast64_t head =
      atomic_fetch_add_explicit(&archive->head, 1, LCIU_memory_order_relaxed);
  LCM_DBG_Assert(head != UINT_FAST64_MAX, "head %lu overflow!\n", head);
  LCM_archive_key_t key = head & ((1 << archive->nbits) - 1);
  LCM_archive_val_t expected_val = LCM_ARCHIVE_EMPTY;
  _Bool succeed = atomic_compare_exchange_strong_explicit(
      &archive->ptr[key].val, &expected_val, value, LCIU_memory_order_relaxed,
      LCIU_memory_order_relaxed);
  if (succeed) {
    *key_ptr = key;
    LCM_DBG_Log(LCM_LOG_DEBUG, "archive", "Archive (%lu, %p) succeed!\n", key,
                (void*)value);
    return LCM_SUCCESS;
  } else {
    LCM_DBG_Log(LCM_LOG_DEBUG, "archive",
                "Archive (%lu, %p) conflicting with %p RETRY!\n", key,
                (void*)value, (void*)expected_val);
    return LCM_RETRY;
  }
}

static inline LCM_archive_val_t LCM_archive_get(LCM_archive_t* archive,
                                                LCM_archive_key_t key)
{
  return atomic_load_explicit(&archive->ptr[key].val,
                              LCIU_memory_order_relaxed);
}

static inline LCM_archive_val_t LCM_archive_remove(LCM_archive_t* archive,
                                                   LCM_archive_key_t key)
{
  // A key will be hold by only one thread
  // so no data race should occur here
  LCM_archive_val_t val =
      atomic_load_explicit(&archive->ptr[key].val, LCIU_memory_order_relaxed);
  atomic_store_explicit(&archive->ptr[key].val, LCM_ARCHIVE_EMPTY,
                        LCIU_memory_order_relaxed);
  LCM_DBG_Log(LCM_LOG_DEBUG, "archive", "Archive remove (%lu, %p)\n", key,
              (void*)val);
  return val;
}

#endif  // LCI_LCM_ARCHIVE_H
