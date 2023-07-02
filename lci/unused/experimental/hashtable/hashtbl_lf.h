#ifndef LF_HASHTBL_H_
#define LF_HASHTBL_H_

#include "hashtbl.h"

extern "C" {
#include "lf_hash.h"
}

class lf_hashtbl : base_hashtbl
{
 public:
  void init() override { tbl_ = qt_hash_create(0); }
  bool insert(const key_type& key, value_type& value) override
  {
    value_type ret;
    ret.v = qt_hash_put(tbl_, (const void*)key, value.v);
    if (ret.v == value.v)
      return true;
    else {
      value = ret;
      qt_hash_remove(tbl_, (const void*)key);
      return false;
    }
  }

 private:
  qt_hash tbl_;
};

#endif
