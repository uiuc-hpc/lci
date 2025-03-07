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
  device_t device;
  endpoint_t endpoint;
  packet_pool_t packet_pool;
  rhandler_registry_t rhandler_registry;
  matching_engine_t matching_engine;
};
}  // namespace lci

#endif  // LCI_CORE_RUNTIME_HPP