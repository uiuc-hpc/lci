#ifndef COCK_HASHTBL_H_
#define COCK_HASHTBL_H_

#include "hashtbl.h"
#include <libcuckoo/cuckoohash_map.hh>

class HashTblCock : HashTblBase {
 public:
  void init() override {}

  bool insert(const key_type& key, value_type& value) override {
    bool ret = true;
    tbl_.upsert(key, [&ret, &value](mpiv_value& v) { value.v = v.v; ret = false; }, value);
    if (!ret) {
      tbl_.erase(key);
    }
    return ret;
  }

 private:
  cuckoohash_map<key_type, value_type> tbl_;
};

#endif
