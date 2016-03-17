#ifndef HASHTBL_H_
#define HASHTBL_H_

#include <utility>
#include <stdlib.h>

using std::pair;
using std::make_pair;

struct mpiv_packet;
struct MPIV_Request;

union mpiv_value {
  void* v;
  mpiv_packet* packet;
  MPIV_Request* request;
};

class base_hashtbl {
 public:
  typedef uint64_t key_type;
  typedef mpiv_value value_type;
  typedef uintptr_t hint_type;

  virtual void init() = 0;
  virtual pair<value_type, hint_type> insert(const key_type& key,
                                             const value_type& value) = 0;
  virtual void erase(const key_type& key, hint_type t) = 0;
};

#endif