#ifndef HASHTBL_H_
#define HASHTBL_H_

#include <utility>
#include <stdlib.h>
#include <stdint.h>

using std::pair;
using std::make_pair;

namespace mpiv {

struct Packet;
struct MPIV_Request;

union mpiv_value {
  void* v;
  Packet* packet;
  MPIV_Request* request;
};

class HashTblBase {
 public:
  typedef uint64_t key_type;
  typedef mpiv_value value_type;

  virtual void init() = 0;
  virtual bool insert(const key_type& key, value_type& value) = 0;
};

typedef uint64_t mpiv_key;

constexpr mpiv_key mpiv_make_key(const int& rank, const int& tag) {
  return (((uint64_t)rank << 32) | tag);
}

#include "hashtbl_arr.h"
#include "hashtbl_cock.h"

template <> struct Config<ConfigType::HASHTBL_ARR> {
  using HashTbl = HashTblArr;
};
template <> struct Config<ConfigType::HASHTBL_COCK> {
  using HasbTbl = HashTblCock;
};

using HashTbl = Config<HashTblCfg>::HashTbl;

}; // namespace mpiv.
#endif
