#ifndef HASHTBL_H_
#define HASHTBL_H_

#include "config.h"

#include <utility>
#include <stdlib.h>

using std::pair;
using std::make_pair;

union mpiv_value {
  void* v;
  mpiv_packet* packet;
  MPIV_Request* request;
};

class base_hashtbl {
 public:
  typedef uint64_t key_type;
  typedef mpiv_value value_type;
  typedef int hint_type;

  virtual void init() = 0;
  virtual pair<value_type, hint_type> insert(const key_type& key, const value_type& value) = 0;
  virtual void erase(const key_type& key, hint_type t) = 0; 
};

#ifdef USE_LF
#include "lf_hashtbl.h"
typedef lf_hashtbl mpiv_hash_tbl;
#endif

#ifdef USE_COCK
#include "cock_hashtbl.h"
typedef cock_hashtbl mpiv_hash_tbl;
#endif

#ifdef USE_ARRAY
#include "arr_hashtbl.h"
typedef arr_hashtbl mpiv_hash_tbl;
#endif

#endif
