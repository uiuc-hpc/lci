#ifndef COCK_HASHTBL_H_
#define COCK_HASHTBL_H_

#include "hashtbl.h"
#include <libcuckoo/cuckoohash_map.hh>

struct ck_hash_val {
  cuckoohash_map<mv_key, mv_value> tbl_;
};

void ck_hash_init(mv_hash** hash) {
  ck_hash_val** h = (ck_hash_val**) hash;
  *h = new ck_hash_val();
}

bool ck_hash_insert(mv_hash* hash, const mv_key& key, mv_value& value) {
  ck_hash_val* h = (ck_hash_val*) hash;
  bool ret = true;
  h->tbl_.upsert(key,
      [&ret, &value](mv_value& v) {
        value = v;
        ret = false;
      },
      value);
  if (!ret) {
    h->tbl_.erase(key);
  }
  return ret;
}

#ifndef hash_init
#define hash_init ck_hash_init
#define hash_insert ck_hash_insert
#endif

#endif
