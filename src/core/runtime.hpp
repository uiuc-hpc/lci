#ifndef LCIXX_CORE_RUNTIME_HPP
#define LCIXX_CORE_RUNTIME_HPP

#include "lcixx_internal.hpp"

namespace lcixx
{
class runtime_impl_t
{
 public:
  using attr_t = runtime_t::attr_t;
  runtime_impl_t(attr_t);
  ~runtime_impl_t();
  int get_rank() const { return rank; }
  int get_nranks() const { return nranks; }
  void initialize(runtime_t);

  runtime_t runtime;
  runtime_t::attr_t attr;
  int rank, nranks;
  net_context_t net_context;
  net_device_t net_device;
};
}  // namespace lcixx

#endif  // LCIXX_CORE_RUNTIME_HPP