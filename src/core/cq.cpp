#include "lci_internal.hpp"

namespace lci
{
comp_t alloc_cq_x::call_impl(runtime_t runtime, void* user_context) const
{
  comp_attr_t attr;
  attr.user_context = user_context;
  comp_t comp;
  comp.p_impl = new cq_impl_t(attr);
  return comp;
}

void free_cq_x::call_impl(comp_t* comp, runtime_t runtime) const
{
  cq_impl_t* p_cq = static_cast<cq_impl_t*>(comp->p_impl);
  delete p_cq;
  comp->p_impl = nullptr;
}

status_t cq_pop_x::call_impl(comp_t comp, runtime_t runtime) const
{
  cq_impl_t* p_cq = static_cast<cq_impl_t*>(comp.p_impl);
  return p_cq->pop();
}

}  // namespace lci