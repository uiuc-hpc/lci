// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

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
  net_context_t default_net_context;
  device_t default_device;
  packet_pool_t default_packet_pool;
  rhandler_registry_t default_rhandler_registry;
  matching_engine_t default_matching_engine;
  matching_engine_t default_coll_matching_engine;
};
}  // namespace lci

#endif  // LCI_CORE_RUNTIME_HPP