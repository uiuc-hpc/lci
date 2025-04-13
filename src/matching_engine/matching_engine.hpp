// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#ifndef LCI_MATCHING_ENGINE_HPP
#define LCI_MATCHING_ENGINE_HPP

namespace lci
{
class matching_engine_impl_t
{
 public:
  using attr_t = matching_engine_t::attr_t;
  attr_t attr;
  using insert_type_t = matching_entry_type_t;
  using key_t = matching_entry_key_t;
  using val_t = matching_entry_val_t;
  matching_engine_impl_t(attr_t attr_) : attr(attr_) {}
  virtual ~matching_engine_impl_t() = default;
  // register the matching engine as a remote handler
  void register_rhandler(runtime_t runtime);
  // get the registered remote handler
  rcomp_t get_rhandler(matching_policy_t policy) const
  {
    return rcomp_base + static_cast<unsigned>(policy);
  }
  // make key from rank and tag
  virtual key_t make_key(int rank, tag_t tag, matching_policy_t policy) const;
  // insert a key-value pair
  virtual val_t insert(key_t key, val_t value, insert_type_t type) = 0;

 private:
  rcomp_t rcomp_base;
};

inline matching_engine_impl_t::key_t matching_engine_impl_t::make_key(
    int rank, tag_t tag, matching_policy_t policy) const
{
  uint64_t key = 0;
  switch (policy) {
    case matching_policy_t::none:
      break;
    case matching_policy_t::rank_only:
      key = set_bits64(key, rank, 32, 0);
      break;
    case matching_policy_t::tag_only:
      key = set_bits64(key, tag, 32, 0);
      break;
    case matching_policy_t::rank_tag:
      key = set_bits64(key, rank, 32, 32);
      key = set_bits64(key, tag, 32, 0);
      break;
    default:
      throw std::runtime_error("Unknown matching policy");
  }
  return key;
}

}  // namespace lci

#include "matching_engine/matching_engine_queue.hpp"
#include "matching_engine/matching_engine_map.hpp"

#endif  // LCI_MATCHING_ENGINE_HPP