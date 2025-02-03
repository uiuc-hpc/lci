#include "lcixx_internal.hpp"

namespace lcixx
{
void comp_signal_x::call_impl(comp_t comp, status_t status,
                              runtime_t runtime) const
{
  comp.p_impl->signal(status);
}

}  // namespace lcixx
