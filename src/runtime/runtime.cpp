// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include "lci_internal.hpp"

namespace lci
{
/*************************************************************
 * runtime: User wrappers
 *************************************************************/

runtime_t alloc_runtime_x::call_impl(size_t packet_return_threshold,
                                     int imm_nbits_tag, int imm_nbits_rcomp,
                                     attr_rdv_protocol_t rdv_protocol,
                                     bool alloc_default_device,
                                     bool alloc_default_packet_pool,
                                     bool alloc_default_matching_engine,
                                     const char* name, void* user_context) const
{
  runtime_attr_t attr;
  attr.packet_return_threshold = packet_return_threshold;
  attr.imm_nbits_tag = imm_nbits_tag;
  attr.imm_nbits_rcomp = imm_nbits_rcomp;
  attr.rdv_protocol = rdv_protocol;
  attr.alloc_default_device = alloc_default_device;
  attr.alloc_default_packet_pool = alloc_default_packet_pool;
  attr.alloc_default_matching_engine = alloc_default_matching_engine;
  attr.name = name;
  attr.user_context = user_context;
  LCI_Assert(attr.imm_nbits_tag + attr.imm_nbits_rcomp <= 31,
             "imm_nbits_tag + imm_nbits_rcomp should be less than 31!\n");
  runtime_t runtime;
  runtime.p_impl = new runtime_impl_t(attr);
  runtime.get_impl()->initialize();
  return runtime;
}

void free_runtime_x::call_impl(runtime_t* runtime) const
{
  delete runtime->p_impl;
  runtime->p_impl = nullptr;
}

runtime_t g_runtime_init_x::call_impl(
    size_t packet_return_threshold, int imm_nbits_tag, int imm_nbits_rcomp,
    attr_rdv_protocol_t rdv_protocol, bool alloc_default_device,
    bool alloc_default_packet_pool, bool alloc_default_matching_engine,
    const char* name, void* user_context) const
{
  runtime_attr_t attr;
  attr.packet_return_threshold = packet_return_threshold;
  attr.imm_nbits_tag = imm_nbits_tag;
  attr.imm_nbits_rcomp = imm_nbits_rcomp;
  attr.rdv_protocol = rdv_protocol;
  attr.alloc_default_device = alloc_default_device;
  attr.alloc_default_packet_pool = alloc_default_packet_pool;
  attr.alloc_default_matching_engine = alloc_default_matching_engine;
  attr.name = name;
  attr.user_context = user_context;
  if (!g_default_runtime.is_empty()) {
    LCI_Warn(
        "The global default runtime has already been initialized. You could "
        "use get_g_runtime().is_empty() to check.\n");
    return g_default_runtime;
  }
  LCI_Assert(attr.imm_nbits_tag + attr.imm_nbits_rcomp <= 31,
             "imm_nbits_tag + imm_nbits_rcomp should be less than 31!\n");
  g_default_runtime.p_impl = new runtime_impl_t(attr);
  g_default_runtime.get_impl()->initialize();
  return g_default_runtime;
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
runtime_impl_t::runtime_impl_t(attr_t attr_) : attr(attr_), rdv_imm_archive(16)
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
  if (attr.rdv_protocol == attr_rdv_protocol_t::auto_select) {
    bool support_putimm = true;
    if (!default_net_context.is_empty()) {
      support_putimm = default_net_context.get_attr_support_putimm();
    }
    attr.rdv_protocol = support_putimm ? attr_rdv_protocol_t::writeimm
                                       : attr_rdv_protocol_t::write;
    LCI_Log(LOG_INFO, "runtime",
            "RDV protocol auto-selected to %s (support_putimm=%d)\n",
            support_putimm ? "writeimm" : "write",
            static_cast<int>(support_putimm));
  }

  if (attr.alloc_default_matching_engine) {
    default_matching_engine = alloc_matching_engine_x().runtime(runtime)();
    // collective matching engine is for internal use only
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

void set_allocator_x::call_impl(allocator_base_t* allocator,
                                runtime_t runtime) const
{
  runtime.get_impl()->allocator = allocator;
}

allocator_base_t* get_allocator_x::call_impl(runtime_t runtime) const
{
  return runtime.get_impl()->allocator;
}

net_context_t get_default_net_context_x::call_impl(runtime_t runtime) const
{
  return runtime.get_impl()->default_net_context;
}

device_t get_default_device_x::call_impl(runtime_t runtime) const
{
  return runtime.get_impl()->default_device;
}

endpoint_t get_default_endpoint_x::call_impl(runtime_t, device_t device) const
{
  return device.get_impl()->default_endpoint;
}

packet_pool_t get_default_packet_pool_x::call_impl(runtime_t runtime) const
{
  return runtime.get_impl()->default_packet_pool;
}

matching_engine_t get_default_matching_engine_x::call_impl(
    runtime_t runtime) const
{
  return runtime.get_impl()->default_matching_engine;
}

}  // namespace lci
