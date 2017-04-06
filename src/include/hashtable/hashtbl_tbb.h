#ifndef TBB_HASHTBL_H_
#define TBB_HASHTBL_H_

#include <tbb/concurrent_hash_map.h>
#include <unordered_map>

struct tbb_hash_val {
  tbb_hash_val() : tbl_(1 << 16) {}
  tbb::concurrent_hash_map<lc_key, lc_value> tbl_;
};

void tbb_hash_init(lc_hash** hash)
{
  tbb_hash_val** h = (tbb_hash_val**)hash;
  *h = new tbb_hash_val();
}

bool tbb_hash_insert(lc_hash* hash, lc_key key, lc_value* value)
{
  tbb::concurrent_hash_map<lc_key, lc_value>::accessor acc;
  tbb_hash_val* h = (tbb_hash_val*)hash;
  if (!h->tbl_.insert(acc, key)) {
    *value = acc->second;
    h->tbl_.erase(acc);
    return false;
  } else {
    acc->second = *value;
    return true;
  }
}

#endif
