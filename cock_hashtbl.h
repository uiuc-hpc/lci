#ifndef COCK_HASHTBL_H_
#define COCK_HASHTBL_H_

#include "hashtbl.h"
#include <libcuckoo/cuckoohash_map.hh>

class cock_hashtbl : base_hashtbl {
 public:
  void init() override {}

  pair<value_type, hint_type> insert(const key_type& key,
                                     const value_type& value) override {
    value_type ret = value;
    tbl_.upsert(key, [&ret](mpiv_value& v) { ret.v = v.v; }, value);
    return make_pair(ret, 0);
  }

  void erase(const key_type& key, hint_type t) override {
    (void)t;
    tbl_.erase(key);
  }

 private:
  cuckoohash_map<key_type, value_type> tbl_;
};

#endif
