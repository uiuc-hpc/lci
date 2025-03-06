#ifndef LCI_MATCHING_ENGINE_HPP
#define LCI_MATCHING_ENGINE_HPP

namespace lci
{
class matching_engine_impl_t
{
 public:
  using attr_t = matching_engine_t::attr_t;
  attr_t attr;

  enum class insert_type_t : unsigned {
    send = 0,
    recv = 1,
  };
  using key_t = uint64_t;
  using val_t = void*;
  matching_engine_impl_t(attr_t attr_) : attr(attr_) {}
  virtual ~matching_engine_impl_t() = default;
  // register the matching engine as a remote handler
  void register_rhandler(runtime_t runtime);
  // get the registered remote handler
  rcomp_t get_rhandler() const { return rcomp; }
  // make key from rank and tag
  virtual key_t make_key(int rank, tag_t tag) const;
  // insert a key-value pair
  virtual val_t insert(key_t key, val_t value, insert_type_t type) = 0;

 private:
  rcomp_t rcomp;
};

inline matching_engine_impl_t::key_t matching_engine_impl_t::make_key(
    int rank, tag_t tag) const
{
  uint64_t key = 0;
  switch (attr.matching_policy) {
    case attr_matching_policy_t::none:
      break;
    case attr_matching_policy_t::rank_only:
      key = set_bits64(key, rank, 32, 0);
      break;
    case attr_matching_policy_t::tag_only:
      key = set_bits64(key, tag, 32, 0);
      break;
    case attr_matching_policy_t::rank_tag:
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