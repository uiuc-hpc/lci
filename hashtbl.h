#ifndef HASHTBL_H_
#define HASHTBL_H_

#include "config.h"

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

inline bool mpiv_tbl_find(const mpiv_key& key, mpiv_value& value) {
    return MPIV.tbl.find(key, value);
}

inline mpiv_value mpiv_tbl_insert(const mpiv_key& key, mpiv_value value) {
    MPIV.tbl.upsert(key, [&value](mpiv_value& v){
        value.v = v.v;
    }, value);
    return value;
}

inline void mpiv_tbl_erase(const mpiv_key& key) {
    MPIV.tbl.erase(key);
}
#endif

#ifdef USE_ARRAY
#include <mutex>
std::mutex m[64];

mpiv_value EMPTY;

inline void mpiv_tbl_init() {
    EMPTY.v = (void*) -1;
    MPIV.tbl = new mpiv_value[64];
    memset(MPIV.tbl, (int) (uintptr_t) (EMPTY.v), 64 * sizeof(mpiv_value));
}

inline bool mpiv_tbl_find(const mpiv_key& key, mpiv_value& value) {
    //return MPIV.tbl.find(key, value);
    m[key].lock();
    if (MPIV.tbl[key].v != EMPTY.v) {
        value = MPIV.tbl[key];
        m[key].unlock();
        return true;
    }
    m[key].unlock();
    return false;
}

inline bool mpiv_tbl_insert(const mpiv_key& key, mpiv_value value) {
    m[key].lock();
    if (MPIV.tbl[key].v != EMPTY.v) {
        m[key].unlock();
        return false;
    } else {
        MPIV.tbl[key].v = value.v;
    }
    m[key].unlock();
    return true;
}

inline void mpiv_tbl_erase(const mpiv_key& key) {
    m[key].lock();
    MPIV.tbl[key].v = EMPTY.v;
    m[key].unlock();
}
#endif

#endif
