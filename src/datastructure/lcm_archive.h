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
  LCM_archive_val_t val;  // 8 bytes
  char padding[LCI_CACHE_LINE - sizeof(LCM_archive_val_t)];
};

struct LCM_archive_t {
  volatile uint64_t head;
  char padding[LCI_CACHE_LINE - sizeof(int64_t)];
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
  archive->head = 0;
  archive->nbits = nbits;

  // Initialize all with EMPTY and clear lock.
  for (size_t i = 0; i < cap; i++) {
    // First are control.
    archive->ptr[i].val = LCM_ARCHIVE_EMPTY;
  }
  return LCM_SUCCESS;
}

static int LCM_archive_fini(LCM_archive_t* archive)
{
  free((void*)archive->ptr);
  archive->ptr = NULL;
  archive->nbits = 0;
  archive->head = 0;
  return LCM_SUCCESS;
}

static inline int LCM_archive_put(LCM_archive_t* archive,
                                  LCM_archive_val_t value,
                                  LCM_archive_key_t* key_ptr)
{
  uint64_t head = __sync_fetch_and_add(&archive->head, 1);
  LCM_DBG_Assert(head != UINT64_MAX, "head %lu overflow!\n", head);
  LCM_archive_key_t key = head & ((1 << archive->nbits) - 1);
  if (archive->ptr[key].val == LCM_ARCHIVE_EMPTY) {
    archive->ptr[key].val = value;
    *key_ptr = key;
    LCM_DBG_Log(LCM_LOG_DEBUG, "archive", "Archive (%lu, %p) succeed!\n", key,
                (void*)value);
    return LCM_SUCCESS;
  } else {
    LCM_DBG_Log(LCM_LOG_DEBUG, "archive",
                "Archive (%lu, %p) conflicting with %p RETRY!\n", key,
                (void*)value, (void*)archive->ptr[key].val);
    return LCM_RETRY;
  }
}

static inline LCM_archive_val_t LCM_archive_get(LCM_archive_t* archive,
                                                LCM_archive_key_t key)
{
  return archive->ptr[key].val;
}

static inline LCM_archive_val_t LCM_archive_remove(LCM_archive_t* archive,
                                                   LCM_archive_key_t key)
{
  // A key will be hold by only one thread
  // so no data race should occur here
  LCM_archive_val_t val = archive->ptr[key].val;
  archive->ptr[key].val = LCM_ARCHIVE_EMPTY;
  LCM_DBG_Log(LCM_LOG_DEBUG, "archive", "Archive remove (%lu, %p)\n", key,
              (void*)val);
  //    LCM_archive_val_t old;
  //    while (true) {
  //        old = __sync_val_compare_and_swap(&(archive.ptr[key].val), val,
  //                                          LCM_ARCHIVE_EMPTY);
  //        if (old == val) {
  //            // succeeded!
  //            break;
  //        } else {
  //            // failed! Try again!
  //            val = old;
  //        }
  //    }
  return val;
}

#endif  // LCI_LCM_ARCHIVE_H
