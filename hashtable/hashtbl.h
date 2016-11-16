#ifndef HASHTBL_H_
#define HASHTBL_H_

#include <stdint.h>
#include <stdlib.h>
#include <utility>

#include "macro.h"

typedef uintptr_t mv_value;
typedef uint64_t mv_key;

MV_INLINE static mv_key mv_make_key(const int& rank, const int& tag) {
  return (((uint64_t)rank << 32) | tag);
}

typedef void* mv_hash;

void hash_init(mv_hash** h);
bool hash_insert(mv_hash* h, const mv_key& key, mv_value& value);

#ifdef HASH_ARR
#include "hashtbl_arr.h"
#endif

#endif
