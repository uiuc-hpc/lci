#include "lci_internal.hpp"

namespace lci
{
rcomp_t register_rcomp_x::call_impl(comp_t comp, runtime_t runtime) const
{
  return runtime.p_impl->rhandler_registry.register_rhandler(
      {rhandler_registry_t::type_t::comp, comp.p_impl});
}

void deregister_rcomp_x::call_impl(rcomp_t rcomp, runtime_t runtime) const
{
  runtime.p_impl->rhandler_registry.deregister_rhandler(rcomp);
}

}  // namespace lci