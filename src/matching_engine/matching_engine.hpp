#ifndef LCI_MATCHING_ENGINE_HPP
#define LCI_MATCHING_ENGINE_HPP

namespace lci
{
class matching_engine_impl_t
{
 public:
  using attr_t = matching_engine_t::attr_t;
  attr_t attr;

  enum class type_t {
    send,
    recv,
  };
  using key_t = uint64_t;
  using val_t = void*;
  matching_engine_impl_t(attr_t attr_) : attr(attr_) {}
  virtual ~matching_engine_impl_t() = default;
  virtual val_t insert(key_t key, val_t value, type_t type) = 0;
};

}  // namespace lci

#include "matching_engine/matching_engine_queue.hpp"

#endif  // LCI_MATCHING_ENGINE_HPP