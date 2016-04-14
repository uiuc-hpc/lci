#ifndef TBB_HASHTBL_H_
#define TBB_HASHTBL_H_

#include "hashtbl.h"
#include <unordered_map>
#include <tbb/concurrent_hash_map.h>

class tbb_hashtbl : base_hashtbl {
 public:

  tbb_hashtbl() : tbl_(1 << 16) {}

  void init() override {}

  bool insert(const key_type& key, value_type& value) override {
    tbb::concurrent_hash_map<key_type, value_type>::accessor acc;
    bool ret = tbl_.insert(acc, {key, value});
    if (!ret) value = acc->second;
    return ret;
  }

 private:
  tbb::concurrent_hash_map<key_type, value_type> tbl_;
};

#endif
