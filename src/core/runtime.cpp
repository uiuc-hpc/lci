#include "lci_internal.hpp"

namespace lci
{
/*************************************************************
 * runtime: User wrappers
 *************************************************************/

runtime_t alloc_runtime_x::call_impl(bool use_reg_cache,
                                     bool use_control_channel,
                                     int packet_return_threshold,
                                     attr_runtime_mode_t runtime_mode,
                                     attr_rdv_protocol_t rdv_protocol) const
{
  runtime_t::attr_t attr;
  attr.use_reg_cache = use_reg_cache;
  attr.use_control_channel = use_control_channel;
  attr.runtime_mode = runtime_mode;
  attr.packet_return_threshold = packet_return_threshold;
  attr.rdv_protocol = rdv_protocol;
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

void g_runtime_init_x::call_impl(bool use_reg_cache, bool use_control_channel,
                                 int packet_return_threshold,
                                 attr_runtime_mode_t runtime_mode,
                                 attr_rdv_protocol_t rdv_protocol) const
{
  LCI_Assert(g_default_runtime.p_impl == nullptr,
             "g_default_runtime has been initialized!\n");
  runtime_t::attr_t attr;
  attr.use_reg_cache = use_reg_cache;
  attr.use_control_channel = use_control_channel;
  attr.runtime_mode = runtime_mode;
  attr.packet_return_threshold = packet_return_threshold;
  attr.rdv_protocol = rdv_protocol;
  g_default_runtime.p_impl = new runtime_impl_t(attr);
  g_default_runtime.p_impl->initialize();
}

void g_runtime_fina_x::call_impl() const
{
  delete g_default_runtime.p_impl;
  g_default_runtime.p_impl = nullptr;
}

/*************************************************************
 * runtime implementation
 *************************************************************/
runtime_impl_t::runtime_impl_t(attr_t attr_) : attr(attr_)
{
  runtime.p_impl = this;
}

void runtime_impl_t::initialize()
{
  if (attr.runtime_mode >= attr_runtime_mode_t::network) {
    net_context = alloc_net_context_x().runtime(runtime)();
    net_device = alloc_net_device_x().runtime(runtime)();
    net_endpoint = alloc_net_endpoint_x().runtime(runtime)();

    if (net_context.get_attr().backend == option_backend_t::ofi &&
        net_context.get_attr().provider_name == "cxi") {
      // special setting for libfabric/cxi
      LCI_Assert(attr.use_reg_cache == false,
                 "The registration cache should be turned off "
                 "for libfabric cxi backend. Use `export LCI_USE_DREG=0`.\n");
      LCI_Assert(attr.use_control_channel == 0,
                 "The progress-specific network endpoint "
                 "for libfabric cxi backend. Use `export "
                 "LCI_ENABLE_PRG_NET_ENDPOINT=0`.\n");
      if (attr.rdv_protocol != attr_rdv_protocol_t::write) {
        attr.rdv_protocol = attr_rdv_protocol_t::write;
        LCI_Warn(
            "Switch LCI_RDV_PROTOCOL to \"write\" "
            "as required by the libfabric cxi backend\n");
      }
    }
  }
  if (attr.runtime_mode == attr_runtime_mode_t::full) {
    packet_pool = alloc_packet_pool_x().runtime(runtime)();
    bind_packet_pool_x(net_device, packet_pool).runtime(runtime)();
  }
}

runtime_impl_t::~runtime_impl_t()
{
  if (attr.runtime_mode == attr_runtime_mode_t::full) {
    unbind_packet_pool_x(net_device).runtime(runtime)();
    free_packet_pool_x(&packet_pool).runtime(runtime)();
  }
  if (attr.runtime_mode >= attr_runtime_mode_t::network) {
    free_net_endpoint_x(&net_endpoint).runtime(runtime)();
    free_net_device_x(&net_device).runtime(runtime)();
    free_net_context_x(&net_context).runtime(runtime)();
  }
}

net_context_t get_default_net_context_x::call_impl(runtime_t runtime) const
{
  return runtime.p_impl->net_context;
}

net_device_t get_default_net_device_x::call_impl(runtime_t runtime) const
{
  return runtime.p_impl->net_device;
}

net_endpoint_t get_default_net_endpoint_x::call_impl(runtime_t runtime) const
{
  return runtime.p_impl->net_endpoint;
}

packet_pool_t get_default_packet_pool_x::call_impl(runtime_t runtime) const
{
  return runtime.p_impl->packet_pool;
}

}  // namespace lci