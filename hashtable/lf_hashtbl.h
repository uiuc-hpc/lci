#ifndef LF_HASHTBL_H_
#define LF_HASHTBL_H_

#include "hashtbl.h"

extern "C" {
#include "lf_hash.h"
}

class lf_hashtbl : base_hashtbl {
 public:
  void init() override { tbl_ = qt_hash_create(0); }

  pair<value_type, hint_type> insert(const key_type& key,
                                     const value_type& value) override {
    value_type ret = value;
    ret.v = qt_hash_put(tbl_, (const void*)key, value.v);
    return make_pair(ret, 0);
  }

  void erase(const key_type& key, hint_type t) override {
    (void)t;
    qt_hash_remove(tbl_, (const void*)key);
  }

 private:
  qt_hash tbl_;
};

#endif
