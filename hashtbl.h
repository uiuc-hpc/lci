#ifndef HASHTBL_H_
#define HASHTBL_H_

#include "config.h"

#include <utility>

using std::pair;
using std::make_pair;

#ifdef USE_LF
inline void mpiv_tbl_init() {
    MPIV.tbl = qt_hash_create(0);
}

inline mpiv_value mpiv_tbl_find(const mpiv_key& key) {
    void* v = qt_hash_get(MPIV.tbl, (const void*) key);
    mpiv_value v1;
    if (v != NULL) {
        v1.v = v;
    }
    return v1;
}

inline mpiv_value mpiv_tbl_insert(const mpiv_key& key, mpiv_value value) {
    value.v = qt_hash_put(MPIV.tbl, (const void*) key, value.v);
    return value;
}

inline void mpiv_tbl_erase(const mpiv_key& key) {
    qt_hash_remove(MPIV.tbl, (const void*) key);
}
#endif

#ifdef USE_COCK
inline void mpiv_tbl_init() {
}

inline pair<mpiv_value, int> mpiv_tbl_insert(const mpiv_key& key, mpiv_value value) {
    MPIV.tbl.upsert(key, [&value](mpiv_value& v){
        value.v = v.v;
    }, value);
    return make_pair(value, 0);
}

inline void mpiv_tbl_erase(const mpiv_key& key, int hint) {
    MPIV.tbl.erase(key);
}
#endif

#ifdef USE_ARRAY
#include <mutex>

class spin_lock {
    std::atomic_flag locked = ATOMIC_FLAG_INIT;
 public:
    void lock() {
        while (locked.test_and_set(std::memory_order_acquire)) { ; }
    }

    void unlock() {
        locked.clear(std::memory_order_release);
    }
};

struct hash_val {
    uint64_t tag;
    void* val;
};

uint64_t EMPTY = (uint64_t) -1;

spin_lock sl[64];

inline void mpiv_tbl_init() {
    MPIV.tbl = new hash_val[64 * 4];
    memset(MPIV.tbl, -1, 64 * 4 * sizeof(hash_val));
}

// default values recommended by http://isthe.com/chongo/tech/comp/fnv/
const uint32_t Prime = 0x01000193; //   16777619
const uint32_t Seed  = 0x811C9DC5; // 2166136261
#define TINY_MASK(x) (((u_int32_t)1<<(x))-1)
#define FNV1_32_INIT ((u_int32_t)2166136261)

inline uint32_t myhash(uint64_t k) {
    const unsigned char* ptr = (const unsigned char*) &k;
    uint32_t hash = Seed;
    hash = (*ptr++ ^ hash) * Prime;
    hash = (*ptr++ ^ hash) * Prime;
    hash = (*ptr++ ^ hash) * Prime;
    hash = (*ptr++ ^ hash) * Prime;
    hash = (*ptr++ ^ hash) * Prime;
    hash = (*ptr++ ^ hash) * Prime;
    hash = (*ptr++ ^ hash) * Prime;
    hash = (*ptr ^ hash) * Prime;

    // Mask into smaller space.
    return (((hash>>6) ^ hash) & TINY_MASK(6));
}

inline pair<mpiv_value, int> mpiv_tbl_insert(const mpiv_key& key, mpiv_value value) {
    uint32_t hash = myhash(key);
    int bucket = hash * 4;
    int slot = 0;
    int empty_slot = -1;

    // if (MPIV.me == 1) printf("inserting %ld %d %d\n", hash, bucket, empty_slot);
    sl[hash].lock();
    while (1) {
        uint64_t tag = MPIV.tbl[slot + bucket].tag;
        if (tag == key) {
            value.v = MPIV.tbl[slot + bucket].val;
            sl[hash].unlock();
            return make_pair(value, slot);
        } else if (tag == EMPTY && empty_slot == -1) {
            empty_slot = slot;
        }
        slot = (slot + 1) % 4;
        if (slot == 0 && empty_slot != -1) break;
    }
    // Find an empty slot.
    MPIV.tbl[empty_slot + bucket].tag = key;
    MPIV.tbl[empty_slot + bucket].val = value.v;
    sl[hash].unlock();
    // if (MPIV.me == 1) printf("inserted %ld %d %d\n", hash, bucket, empty_slot);
    return make_pair(value, empty_slot);
}

inline void mpiv_tbl_erase(const mpiv_key& key, int hint) {
    uint32_t hash = myhash(key);
    int bucket = hash * 4;
    // if (MPIV.me == 1) printf("erasing %ld %d %d\n", hash, bucket, hint);
    sl[hash].lock();
    MPIV.tbl[bucket + hint].tag = EMPTY;
    sl[hash].unlock();
    // if (MPIV.me == 1) printf("erased %ld %d %d\n", hash, bucket, hint);
}
#endif

#endif
