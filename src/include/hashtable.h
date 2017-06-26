#ifndef ARR_HASHTBL_H_
#define ARR_HASHTBL_H_

#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdlib.h>

typedef uintptr_t lc_value;
typedef uint64_t lc_key;
typedef void* lc_hash;

#include <assert.h>
#include <stdlib.h>

#include "lc/lock.h"

#define EMPTY ((uint64_t)-1)
#define TBL_BIT_SIZE 16
#define TBL_WIDTH 4

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
} hash_val;

void lc_hash_create(lc_hash** h);
void lc_hash_destroy(lc_hash* h);
int lc_hash_insert2(lc_hash* h, lc_key key, lc_value* value);
int lc_hash_insert(lc_hash* h, lc_key key, lc_value* value, int type);

#endif
