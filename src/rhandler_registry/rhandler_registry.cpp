// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include "lci_internal.hpp"

namespace lci
{
rcomp_t reserve_rcomps_x::call_impl(rcomp_t n, runtime_t runtime) const
{
  return runtime.p_impl->default_rhandler_registry.reserve(n);
}

rcomp_t register_rcomp_x::call_impl(comp_t comp, runtime_t runtime,
                                    rcomp_t rcomp_in) const
{
  if (rcomp_in) {
    runtime.p_impl->default_rhandler_registry.register_rhandler(
        rcomp_in, {rhandler_registry_t::type_t::comp, comp.p_impl});
    return rcomp_in;
  } else {
    return runtime.p_impl->default_rhandler_registry.register_rhandler(
        {rhandler_registry_t::type_t::comp, comp.p_impl});
  }
}

void deregister_rcomp_x::call_impl(rcomp_t rcomp, runtime_t runtime) const
{
  runtime.p_impl->default_rhandler_registry.deregister_rhandler(rcomp);
}

}  // namespace lci