#include "lcixx_internal.hpp"

namespace lcixx
{
void comp_signal_x::call() const { comp_.p_impl->signal(status_); }

}  // namespace lcixx
