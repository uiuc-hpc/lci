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
  void initialize();

  runtime_t runtime;
  runtime_t::attr_t attr;
  net_context_t net_context;
  net_device_t net_device;
  net_endpoint_t net_endpoint;
};
}  // namespace lcixx

#endif  // LCIXX_CORE_RUNTIME_HPP