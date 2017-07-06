#ifndef ARR_HASHTBL_H_
#define ARR_HASHTBL_H_

#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdlib.h>
#include "ptmalloc.h"

typedef uintptr_t lc_value;
typedef uint64_t lc_key;
typedef void lc_hash;

#include <assert.h>
#include <stdlib.h>

#include "lc/macro.h"
#include "lc/lock.h"

#define EMPTY ((uint64_t)-1)
#define TBL_BIT_SIZE 16
#define TBL_WIDTH 4

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hash_val {
  union {
    struct {
      lc_key tag;
      lc_value val;
    } entry;
    struct {
      volatile int lock;
      struct hash_val* next;
    } control;
  };
  // NOTE: This must be aligned to 16, make sure TBL_WDITH is 4,
  // So they will fit in a cache line.
} hash_val_t;

enum insert_type {
  CLIENT,
  SERVER
};

LC_EXPORT
void lc_hash_create(lc_hash** h);

LC_EXPORT
void lc_hash_destroy(lc_hash* h);

LC_EXPORT
int lc_hash_insert(lc_hash* h, lc_key key, lc_value* value,
                   enum insert_type type);

#ifdef __cplusplus
}
#endif

// default values recommended by http://isthe.com/chongo/tech/comp/fnv/
static const uint32_t Prime = 0x01000193; //   16777619
static const uint32_t Seed = 0x811C9DC5;  // 2166136261
#define TINY_MASK(x) (((uint32_t)1 << (x)) - 1)
#define FNV1_32_INIT ((uint32_t)2166136261)

LC_INLINE uint32_t myhash(const uint64_t k)
{
  uint32_t hash = ((k & 0xff) ^ Seed) * Prime;
  hash = (((k >> 8) & 0xff) ^ hash) * Prime;
  hash = (((k >> 16) & 0xff) ^ hash) * Prime;
  hash = (((k >> 24) & 0xff) ^ hash) * Prime;
  hash = (((k >> 32) & 0xff) ^ hash) * Prime;
  hash = (((k >> 40) & 0xff) ^ hash) * Prime;
  hash = (((k >> 48) & 0xff) ^ hash) * Prime;
  hash = (((k >> 56) & 0xff) ^ hash) * Prime;

  // Mask into smaller space.
  return (((hash >> TBL_BIT_SIZE) ^ hash) & TINY_MASK(TBL_BIT_SIZE));
}

#endif
