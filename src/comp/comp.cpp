#include "lci_internal.hpp"

namespace lci
{
comp_t alloc_sync_x::call_impl(int threshold, runtime_t runtime,
                               void* user_context) const
{
  comp_attr_t attr;
  attr.user_context = user_context;
  comp_t comp;
  comp.p_impl = new sync_t(attr, threshold);
  return comp;
}

comp_t alloc_cq_x::call_impl(runtime_t runtime, void* user_context) const
{
  comp_attr_t attr;
  attr.user_context = user_context;
  comp_t comp;
  comp.p_impl = new cq_t(attr);
  return comp;
}

void free_comp_x::call_impl(comp_t* comp, runtime_t runtime) const
{
  delete comp->p_impl;
  comp->p_impl = nullptr;
}

}  // namespace lci