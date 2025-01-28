#include "lcixx_internal.hpp"

namespace lcixx
{
void register_rcomp_x::call() const
{
  runtime_t runtime = runtime_.get_value_or(g_default_runtime);
  *rcomp_ = runtime.p_impl->rcomp_registry.register_rcomp(comp_);
}

}  // namespace lcixx