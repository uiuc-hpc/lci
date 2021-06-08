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

#define LCM_ARCHIVE_NTRY 3
#define LCM_ARCHIVE_EMPTY 0

typedef uint64_t LCM_archive_key_t;
typedef uintptr_t LCM_archive_val_t;

struct LCM_archive_entry_t {
  LCM_archive_val_t val;   // 8 bytes
  char padding[64-sizeof(LCM_archive_val_t)];
};

struct LCM_archive_t {
  volatile struct LCM_archive_entry_t* ptr;
  int nbits;
};
typedef struct LCM_archive_t LCM_archive_t;

static int LCM_archive_init(LCM_archive_t* archive, int nbits);
static int LCM_archive_fini(LCM_archive_t* archive);

static inline int LCM_archive_put(LCM_archive_t archive,
                                  LCM_archive_val_t value,
                                  LCM_archive_key_t* key);
static inline LCM_archive_val_t LCM_archive_get(LCM_archive_t archive,
                                                LCM_archive_key_t key);
static inline LCM_archive_val_t LCM_archive_remove(LCM_archive_t archive,
                                                   LCM_archive_key_t key);

#ifdef __cplusplus
}
#endif

static int LCM_archive_init(LCM_archive_t* archive, int nbits)
{
  assert(sizeof(struct LCM_archive_entry_t) == 64);
  size_t cap = 1 << nbits;
  int ret = posix_memalign((void **) &archive->ptr, 64, cap * sizeof(struct LCM_archive_entry_t));
  LCM_Assert(ret == 0, "Memory allocation failed!\n");
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
  free((void*) archive->ptr);
  archive->ptr = NULL;
  archive->nbits = 0;
  return LCM_SUCCESS;
}

static inline int LCM_archive_put(LCM_archive_t archive,
                                  LCM_archive_val_t value,
                                  LCM_archive_key_t* key_ptr)
{
  LCM_archive_key_t key = (value >> 6) & ((1 << archive.nbits) - 1);
  for (int i = 1; i <= LCM_ARCHIVE_NTRY; ++i) {
    LCM_DBG_Log(LCM_LOG_DEBUG, "Archive try (%lu, %lu)\n", key, value);
    if (archive.ptr[key].val == LCM_ARCHIVE_EMPTY) {
      if (__sync_bool_compare_and_swap(&(archive.ptr[key].val), LCM_ARCHIVE_EMPTY,
                                       value)) {
        *key_ptr = key;
        LCM_DBG_Log(LCM_LOG_DEBUG, "Archive (%lu, %lu) succeed!\n", key, value);
        return LCM_SUCCESS;
      }
    }
    // quadratic probe
    key += i * i;
  }
  LCM_DBG_Log(LCM_LOG_DEBUG, "Archive %lu RETRY!\n", value);
  return LCM_RETRY;
}

static inline LCM_archive_val_t LCM_archive_get(LCM_archive_t archive,
                                                LCM_archive_key_t key)
{
  return archive.ptr[key].val;
}

static inline LCM_archive_val_t LCM_archive_remove(LCM_archive_t archive,
                                                   LCM_archive_key_t key)
{
  // A key will be hold by only one thread
  // so no data race should occur here
  LCM_archive_val_t val = archive.ptr[key].val;
  archive.ptr[key].val = LCM_ARCHIVE_EMPTY;
  LCM_DBG_Log(LCM_LOG_DEBUG, "Archive remove (%lu, %lu)\n", key, val);
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


#endif//LCI_LCM_ARCHIVE_H
