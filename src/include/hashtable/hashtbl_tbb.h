#ifndef TBB_HASHTBL_H_
#define TBB_HASHTBL_H_

#include <tbb/concurrent_hash_map.h>
#include <unordered_map>

struct tbb_hash_val {
  tbb_hash_val() : tbl_(1 << 16) {}
  tbb::concurrent_hash_map<mv_key, mv_value> tbl_;
};

void tbb_hash_init(mv_hash** hash)
{
  tbb_hash_val** h = (tbb_hash_val**)hash;
  *h = new tbb_hash_val();
}

bool tbb_hash_insert(mv_hash* hash, mv_key key, mv_value* value)
{
  tbb::concurrent_hash_map<mv_key, mv_value>::accessor acc;
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
