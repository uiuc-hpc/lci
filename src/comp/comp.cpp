// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include "lci_internal.hpp"

namespace lci
{
comp_t alloc_sync_x::call_impl(runtime_t, int threshold, bool zero_copy_am,
                               void* user_context) const
{
  comp_attr_t attr;
  memset(&attr, 0, sizeof(attr));
  attr.zero_copy_am = zero_copy_am;
  attr.user_context = user_context;
  comp_t comp;
  comp.p_impl = new sync_t(attr, threshold);
  return comp;
}

comp_t alloc_cq_x::call_impl(runtime_t, int default_length, bool zero_copy_am,
                             attr_cq_type_t cq_type, void* user_context) const
{
  comp_attr_t attr;
  memset(&attr, 0, sizeof(attr));
  attr.zero_copy_am = zero_copy_am;
  attr.cq_type = cq_type;
  attr.user_context = user_context;
  comp_t comp;
  comp.p_impl = new cq_t(attr, default_length);
  return comp;
}

comp_t alloc_handler_x::call_impl(comp_handler_t handler, runtime_t,
                                  bool zero_copy_am, void* user_context) const
{
  comp_attr_t attr;
  memset(&attr, 0, sizeof(attr));
  attr.zero_copy_am = zero_copy_am;
  attr.user_context = user_context;
  comp_t comp;
  comp.p_impl = new handler_t(attr, handler);
  return comp;
}

comp_t alloc_graph_x::call_impl(comp_t comp, void* user_context,
                                runtime_t) const
{
  comp_attr_t attr;
  memset(&attr, 0, sizeof(attr));
  attr.user_context = user_context;
  comp_t ret;
  ret.p_impl = new graph_t(attr, comp);
  return ret;
}

void free_comp_x::call_impl(comp_t* comp, runtime_t) const
{
  delete comp->p_impl;
  comp->p_impl = nullptr;
}

void comp_signal_x::call_impl(comp_t comp, status_t status, runtime_t) const
{
  comp.p_impl->signal(status);
}

}  // namespace lci