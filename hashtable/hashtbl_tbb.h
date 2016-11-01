#ifndef TBB_HASHTBL_H_
#define TBB_HASHTBL_H_

#include "hashtbl.h"
#include <tbb/concurrent_hash_map.h>
#include <unordered_map>
class tbb_hashtbl : base_hashtbl {
 public:
  tbb_hashtbl() : tbl_(1 << 16) {}

  void init() override {}

  bool insert(const key_type& key, value_type& value) override {
    tbb::concurrent_hash_map<key_type, value_type>::accessor acc;
    if (!tbl_.insert(acc, key)) {
      value = acc->second;
      tbl_.erase(acc);
      return false;
    } else {
      acc->second = value;
      return true;
    }
  }

 private:
  tbb::concurrent_hash_map<key_type, value_type> tbl_;
};

#if 0
#include <folly/AtomicHashMap.h>
class folly_hashtbl : base_hashtbl {
 public:
  folly_hashtbl() : tbl_(1 << 16) {}
  void init() override {}

  bool insert(const key_type& key, value_type& value) override {
    auto p = tbl_.insert(key, value);
    if (!p.second) {
      value = (*(p.first)).second;
      tbl_.erase(key);
      return false;
    }
    return true;
  }

 private:
  folly::AtomicHashMap<key_type, value_type> tbl_;
};
#endif

#endif
