#include "lcixx_internal.hpp"

namespace lcixx
{
comp_t alloc_cq_x::call_impl(runtime_t runtime) const
{
  comp_t comp;
  comp.p_impl = new cq_impl_t();
  return comp;
}

void free_cq_x::call_impl(comp_t* comp, runtime_t runtime) const
{
  cq_impl_t* p_cq = static_cast<cq_impl_t*>(comp->p_impl);
  delete p_cq;
  comp->p_impl = nullptr;
}

std::tuple<error_t, status_t> cq_pop_x::call_impl(comp_t comp,
                                                  runtime_t runtime) const
{
  error_t error;
  cq_impl_t* p_cq = static_cast<cq_impl_t*>(comp.p_impl);
  status_t status;
  bool succeed = p_cq->pop(&status);
  if (!succeed) {
    error.reset(errorcode_t::retry);
  } else {
    error.reset(errorcode_t::ok);
  }
  return {error, status};
}

}  // namespace lcixx