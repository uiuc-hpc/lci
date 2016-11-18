#ifndef HASHTBL_H_
#define HASHTBL_H_

#include <stdint.h>
#include <stdlib.h>

#include "macro.h"

typedef uintptr_t mv_value;
typedef uint64_t mv_key;

extern "C" {
    typedef void* mv_hash;
    void hash_init(mv_hash** h);
    int hash_insert(mv_hash* h, mv_key key, mv_value* value);
}

#endif
