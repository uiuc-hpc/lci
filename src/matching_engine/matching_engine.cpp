// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include "lci_internal.hpp"

namespace lci
{
void matching_engine_impl_t::register_rhandler(runtime_t runtime)
{
  auto& rhandler_registry = runtime.p_impl->default_rhandler_registry;
  rcomp_base =
      rhandler_registry.reserve(static_cast<unsigned>(matching_policy_t::max));
  for (unsigned i = 0; i < static_cast<unsigned>(matching_policy_t::max); i++) {
    rhandler_registry.register_rhandler(
        rcomp_base + i,
        {rhandler_registry_t::type_t::matching_engine, this, i});
  }
}

matching_engine_t alloc_matching_engine_x::call_impl(
    runtime_t runtime, attr_matching_engine_type_t matching_engine_type,
    void* user_context) const
{
  matching_engine_t matching_engine;
  matching_engine_attr_t attr;
  attr.matching_engine_type = matching_engine_type;
  attr.user_context = user_context;
  switch (matching_engine_type) {
    case attr_matching_engine_type_t::map:
      matching_engine.p_impl = new matching_engine_map_t(attr);
      break;
    case attr_matching_engine_type_t::queue:
      matching_engine.p_impl = new matching_engine_queue_t(attr);
      break;
    default:
      throw std::runtime_error("Invalid matching engine type");
  }
  matching_engine.get_impl()->register_rhandler(runtime);
  return matching_engine;
}

void free_matching_engine_x::call_impl(matching_engine_t* matching_engine,
                                       runtime_t) const
{
  delete matching_engine->p_impl;
  matching_engine->p_impl = nullptr;
}

matching_entry_val_t matching_engine_insert_x::call_impl(
    matching_engine_t matching_engine, matching_entry_key_t key,
    matching_entry_val_t value, matching_entry_type_t type, runtime_t) const
{
  return matching_engine.get_impl()->insert(key, value, type);
}

}  // namespace lci
