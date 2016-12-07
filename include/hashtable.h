#ifndef ARR_HASHTBL_H_
#define ARR_HASHTBL_H_

#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdlib.h>

typedef uintptr_t mv_value;
typedef uint64_t mv_key;
typedef void* mv_hash;

#include <assert.h>
#include <stdlib.h>

#include "lock.h"
#include "macro.h"

#define EMPTY ((uint64_t)-1)
#define TBL_BIT_SIZE 9
#define TBL_WIDTH  4

uint32_t myhash(const uint64_t k);

typedef struct hash_val {
  union {
    struct {
      mv_key tag;
      mv_value val;
    } entry;
    struct {
      volatile int lock __attribute__((aligned(8)));
      struct hash_val* next;
    } control;
  };
} hash_val __attribute__((aligned(64)));

hash_val* create_table(size_t num_rows);
void mv_hash_init(mv_hash** h);
int mv_hash_insert(mv_hash* h, mv_key key, mv_value* value);

#endif
