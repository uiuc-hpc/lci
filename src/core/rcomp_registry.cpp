#include "lci_internal.hpp"

namespace lci
{
rcomp_t register_rcomp_x::call_impl(comp_t comp, runtime_t runtime) const
{
  return runtime.p_impl->rcomp_registry.register_rcomp(comp);
}

void deregister_rcomp_x::call_impl(rcomp_t rcomp, runtime_t runtime) const
{
  runtime.p_impl->rcomp_registry.deregister_rcomp(rcomp);
}

}  // namespace lci