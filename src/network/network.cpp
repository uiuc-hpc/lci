// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include "lci_internal.hpp"

namespace lci
{
std::atomic<int> device_impl_t::g_ndevices(0);
std::atomic<int> endpoint_impl_t::g_nendpoints(0);

net_context_impl_t::net_context_impl_t(runtime_t runtime_, attr_t attr_)
    : attr(attr_), runtime(runtime_)
{
  net_context.p_impl = this;
}

device_impl_t::device_impl_t(net_context_t context_, attr_t attr_)
    : attr(attr_), net_context(context_), nrecvs_posted(0)
{
  attr.uid = g_ndevices++;
  runtime = net_context.p_impl->runtime;
  device.p_impl = this;
}

device_impl_t::~device_impl_t() = default;

endpoint_t device_impl_t::alloc_endpoint(endpoint_t::attr_t attr)
{
  endpoint_t ret = alloc_endpoint_impl(attr);
  endpoints.push_back(ret);
  return ret;
}

endpoint_impl_t::endpoint_impl_t(device_t device_, attr_t attr_)
    : runtime(device_.p_impl->runtime), device(device_), attr(attr_)
{
  attr.uid = g_nendpoints++;
  endpoint.p_impl = this;
}

/*************************************************************************************
 * Interface implementations
 * **********************************************************************************/

net_context_t alloc_net_context_x::call_impl(
    runtime_t runtime, attr_backend_t backend, std::string ofi_provider_name,
    size_t max_msg_size, size_t max_inject_size, int ibv_gid_idx,
    bool ibv_force_gid_auto_select, attr_ibv_odp_strategy_t ibv_odp_strategy,
    attr_ibv_prefetch_strategy_t ibv_prefetch_strategy,
    void* user_context) const
{
  net_context_t net_context;

  net_context_t::attr_t attr;
  attr.backend = backend;
  attr.ofi_provider_name = ofi_provider_name;
  attr.max_msg_size = max_msg_size;
  attr.user_context = user_context;
  attr.ibv_gid_idx = ibv_gid_idx;
  attr.ibv_force_gid_auto_select = ibv_force_gid_auto_select;
  attr.ibv_odp_strategy = ibv_odp_strategy;
  attr.ibv_prefetch_strategy = ibv_prefetch_strategy;
  attr.max_inject_size = max_inject_size;

  switch (attr.backend) {
    case attr_backend_t::none:
      break;
    case attr_backend_t::ibv:
#ifdef LCI_BACKEND_ENABLE_IBV
      net_context.p_impl = new ibv_net_context_impl_t(runtime, attr);
#else
      LCI_Assert(false, "IBV backend is not enabled");
#endif
      break;
    case attr_backend_t::ofi:
#ifdef LCI_BACKEND_ENABLE_OFI
      net_context.p_impl = new ofi_net_context_impl_t(runtime, attr);
#else
      LCI_Assert(false, "OFI backend is not enabled");
#endif
      break;
    case attr_backend_t::ucx:
#ifdef LCI_BACKEND_ENABLE_UCX
      throw std::runtime_error("UCX backend is not implemented\n");
      // ret.p_impl = new ucx_net_context_impl_t(runtime, attr);
#else
      LCI_Assert(false, "UCX backend is not enabled");
#endif
      break;
    default:
      LCI_Assert(false, "Unsupported backend %d", attr.backend);
  }
  return net_context;
}

void free_net_context_x::call_impl(net_context_t* net_context, runtime_t) const
{
  delete net_context->p_impl;
  net_context->p_impl = nullptr;
}

device_t alloc_device_x::call_impl(
    runtime_t runtime, size_t net_max_sends, size_t net_max_recvs,
    size_t net_max_cqes, uint64_t ofi_lock_mode, bool alloc_default_endpoint,
    attr_ibv_td_strategy_t ibv_td_strategy, void* user_context,
    net_context_t net_context, packet_pool_t packet_pool) const
{
  device_t::attr_t attr;
  attr.net_max_sends = net_max_sends;
  attr.net_max_recvs = net_max_recvs;
  attr.net_max_cqes = net_max_cqes;
  attr.ofi_lock_mode = ofi_lock_mode;
  attr.alloc_default_endpoint = alloc_default_endpoint;
  attr.ibv_td_strategy = ibv_td_strategy;
  attr.user_context = user_context;
  auto device = net_context.p_impl->alloc_device(attr);
  if (!packet_pool.is_empty()) {
    device.get_impl()->bind_packet_pool(packet_pool);
  }
  if (attr.alloc_default_endpoint)
    device.get_impl()->default_endpoint =
        alloc_endpoint_x().runtime(runtime).device(device)();
  if (device.get_attr_uid() == 0) {
    bootstrap::set_device(device);
  }
  return device;
}

void free_device_x::call_impl(device_t* device, runtime_t runtime) const
{
  if (device->get_attr_uid() == 0) {
    bootstrap::set_device(device_t());
  }
  endpoint_t default_endpoint = device->get_impl()->default_endpoint;
  if (!default_endpoint.is_empty()) {
    free_endpoint_x(&default_endpoint).runtime(runtime)();
  }
  delete device->p_impl;
  device->p_impl = nullptr;
}

endpoint_t alloc_endpoint_x::call_impl(runtime_t, void* user_context,
                                       device_t device) const
{
  endpoint_t::attr_t attr;
  attr.user_context = user_context;
  auto endpoint = device.p_impl->alloc_endpoint(attr);
  barrier_x()
      .runtime(endpoint.get_impl()->runtime)
      .device(endpoint.get_impl()->device)
      .endpoint(endpoint)
      .comp_semantic(comp_semantic_t::network)();
  return endpoint;
}

void free_endpoint_x::call_impl(endpoint_t* endpoint, runtime_t) const
{
  barrier_x()
      .runtime(endpoint->get_impl()->runtime)
      .device(endpoint->get_impl()->device)
      .endpoint(*endpoint)
      .comp_semantic(comp_semantic_t::network)();
  delete endpoint->p_impl;
  endpoint->p_impl = nullptr;
}

}  // namespace lci