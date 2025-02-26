#ifndef LCI_CORE_RUNTIME_HPP
#define LCI_CORE_RUNTIME_HPP

#include "lci_internal.hpp"

namespace lci
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
  packet_pool_t packet_pool;
  rcomp_registry_t rcomp_registry;
  matching_engine_t matching_engine;
};
}  // namespace lci

#endif  // LCI_CORE_RUNTIME_HPP