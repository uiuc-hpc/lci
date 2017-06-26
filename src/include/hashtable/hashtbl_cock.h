#ifndef COCK_HASHTBL_H_
#define COCK_HASHTBL_H_

#include <libcuckoo/cuckoohash_map.hh>

struct ck_hash_val {
  cuckoohash_map<lc_key, lc_value> tbl_;
};

void ck_hash_init(lc_hash** hash)
{
  ck_hash_val** h = (ck_hash_val**)hash;
  *h = new ck_hash_val();
}

bool ck_hash_insert(lc_hash* hash, lc_key key, lc_value* value, int type)
{
  ck_hash_val* h = (ck_hash_val*)hash;
  bool ret = true;
  h->tbl_.upsert(key,
                 [&ret, &value](lc_value& v) {
                   *value = v;
                   ret = false;
                 },
                 *value);
  if (!ret) {
    h->tbl_.erase(key);
  }
  return ret;
}

#endif
