// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include "lci_internal.hpp"

namespace lci
{
/*************************************************************
 * runtime: User wrappers
 *************************************************************/

runtime_t alloc_runtime_x::call_impl(size_t packet_return_threshold,
                                     int imm_nbits_tag, int imm_nbits_rcomp,
                                     bool alloc_default_device,
                                     bool alloc_default_packet_pool,
                                     bool alloc_default_matching_engine,
                                     attr_rdv_protocol_t rdv_protocol,
                                     void* user_context) const
{
  LCI_Assert(imm_nbits_tag + imm_nbits_rcomp <= 31,
             "imm_nbits_tag + imm_nbits_rcomp should be less than 31!\n");
  runtime_t::attr_t attr;
  // attr.use_reg_cache = use_reg_cache;
  // attr.use_control_channel = use_control_channel;
  attr.packet_return_threshold = packet_return_threshold;
  attr.imm_nbits_rcomp = imm_nbits_rcomp;
  attr.imm_nbits_tag = imm_nbits_tag;
  attr.rdv_protocol = rdv_protocol;
  attr.user_context = user_context;
  attr.alloc_default_device = alloc_default_device;
  attr.alloc_default_packet_pool = alloc_default_packet_pool;
  attr.alloc_default_matching_engine = alloc_default_matching_engine;
  runtime_t runtime;
  runtime.p_impl = new runtime_impl_t(attr);
  runtime.p_impl->initialize();
  return runtime;
}

void free_runtime_x::call_impl(runtime_t* runtime) const
{
  delete runtime->p_impl;
  runtime->p_impl = nullptr;
}

void g_runtime_init_x::call_impl(size_t packet_return_threshold,
                                 int imm_nbits_tag, int imm_nbits_rcomp,
                                 bool alloc_default_device,
                                 bool alloc_default_packet_pool,
                                 bool alloc_default_matching_engine,
                                 attr_rdv_protocol_t rdv_protocol) const
{
  LCI_Assert(g_default_runtime.p_impl == nullptr,
             "g_default_runtime has been initialized!\n");
  LCI_Assert(imm_nbits_tag + imm_nbits_rcomp <= 31,
             "imm_nbits_tag + imm_nbits_rcomp should be less than 31!\n");
  runtime_t::attr_t attr;
  // attr.use_reg_cache = use_reg_cache;
  // attr.use_control_channel = use_control_channel;
  attr.packet_return_threshold = packet_return_threshold;
  attr.imm_nbits_rcomp = imm_nbits_rcomp;
  attr.imm_nbits_tag = imm_nbits_tag;
  attr.rdv_protocol = rdv_protocol;
  attr.user_context = nullptr;
  attr.alloc_default_device = alloc_default_device;
  attr.alloc_default_packet_pool = alloc_default_packet_pool;
  attr.alloc_default_matching_engine = alloc_default_matching_engine;
  g_default_runtime.p_impl = new runtime_impl_t(attr);
  g_default_runtime.p_impl->initialize();
}

void g_runtime_fina_x::call_impl() const
{
  delete g_default_runtime.p_impl;
  g_default_runtime.p_impl = nullptr;
}

runtime_t get_g_runtime_x::call_impl() const { return g_default_runtime; }

/*************************************************************
 * runtime implementation
 *************************************************************/
runtime_impl_t::runtime_impl_t(attr_t attr_) : attr(attr_)
{
  runtime.p_impl = this;
}

void runtime_impl_t::initialize()
{
  attr.max_imm_tag = (1ULL << attr.imm_nbits_tag) - 1;
  attr.max_imm_rcomp = (1ULL << attr.imm_nbits_rcomp) - 1;
  // We can support up to 32-bit tag for now.
  // TODO: technically we can support up to 64-bit tag, but we need to change
  // the matching engine key format.
  attr.max_tag = std::numeric_limits<uint32_t>::max();
  attr.max_rcomp = std::numeric_limits<rcomp_t>::max();
  default_net_context = alloc_net_context_x().runtime(runtime)();

  if (attr.alloc_default_matching_engine) {
    default_matching_engine = alloc_matching_engine_x().runtime(runtime)();
    default_coll_matching_engine = alloc_matching_engine_x().runtime(runtime)();
  }
  if (default_net_context.get_attr().backend == attr_backend_t::ofi &&
      default_net_context.get_attr().ofi_provider_name == "cxi") {
    // special setting for libfabric/cxi
    // LCI_Assert(attr.use_reg_cache == false,
    //            "The registration cache should be turned off "
    //            "for libfabric cxi backend. Use `export LCI_USE_DREG=0`.\n");
    // LCI_Assert(attr.use_control_channel == 0,
    //            "The progress-specific network endpoint "
    //            "for libfabric cxi backend. Use `export "
    //            "LCI_ENABLE_PRG_NET_ENDPOINT=0`.\n");
    if (attr.rdv_protocol != attr_rdv_protocol_t::write) {
      attr.rdv_protocol = attr_rdv_protocol_t::write;
      LCI_Warn(
          "Switch LCI_RDV_PROTOCOL to \"write\" "
          "as required by the libfabric cxi backend\n");
    }
  }
  if (attr.alloc_default_packet_pool) {
    default_packet_pool = alloc_packet_pool_x().runtime(runtime)();
  }
  if (attr.alloc_default_device) {
    default_device = alloc_device_x().runtime(runtime)();
  }
}

runtime_impl_t::~runtime_impl_t()
{
  if (!default_device.is_empty()) {
    free_device_x(&default_device).runtime(runtime)();
  }
  if (!default_packet_pool.is_empty()) {
    free_packet_pool_x(&default_packet_pool).runtime(runtime)();
  }
  if (!default_net_context.is_empty()) {
    free_net_context_x(&default_net_context).runtime(runtime)();
  }
  if (!default_matching_engine.is_empty()) {
    free_matching_engine_x(&default_matching_engine).runtime(runtime)();
    free_matching_engine_x(&default_coll_matching_engine).runtime(runtime)();
  }
}

net_context_t get_default_net_context_x::call_impl(runtime_t runtime) const
{
  return runtime.p_impl->default_net_context;
}

device_t get_default_device_x::call_impl(runtime_t runtime) const
{
  return runtime.p_impl->default_device;
}

endpoint_t get_default_endpoint_x::call_impl(runtime_t, device_t device) const
{
  return device.p_impl->default_endpoint;
}

packet_pool_t get_default_packet_pool_x::call_impl(runtime_t runtime) const
{
  return runtime.p_impl->default_packet_pool;
}

matching_engine_t get_default_matching_engine_x::call_impl(
    runtime_t runtime) const
{
  return runtime.p_impl->default_matching_engine;
}

}  // namespace lci