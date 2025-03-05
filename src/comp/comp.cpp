#include "lci_internal.hpp"

namespace lci
{
comp_t alloc_comp_x::call_impl(runtime_t runtime, attr_comp_type_t comp_type,
                               int cq_default_length, int sync_threshold,
                               void* user_context) const
{
  comp_attr_t attr;
  attr.user_context = user_context;
  switch (comp_type) {
    case attr_comp_type_t::cq:
      attr.cq_default_length = cq_default_length;
      return new cq_t(attr);
    case attr_comp_type_t::sync:
      attr.sync_threshold = sync_threshold;
      return new sync_t(attr);
    default:
      LCI_Assert(false, "unknown comp type");
      return comp_t();
  }
}

comp_t alloc_sync_x::call_impl(runtime_t runtime, int threshold,
                               void* user_context) const
{
  return alloc_comp_x()
      .comp_type(attr_comp_type_t::sync)
      .sync_threshold(threshold)
      .runtime(runtime)
      .user_context(user_context)
      .call();
}

comp_t alloc_cq_x::call_impl(runtime_t runtime, int default_length,
                             void* user_context) const
{
  return alloc_comp_x()
      .comp_type(attr_comp_type_t::cq)
      .cq_default_length(default_length)
      .runtime(runtime)
      .user_context(user_context)
      .call();
}

void free_comp_x::call_impl(comp_t* comp, runtime_t runtime) const
{
  delete comp->p_impl;
  comp->p_impl = nullptr;
}

}  // namespace lci