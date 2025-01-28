#include "lcixx_internal.hpp"

namespace lcixx
{
/*************************************************************
 * runtime: User wrappers
 *************************************************************/

void alloc_runtime_x::call() const
{
  global_initialize();
  runtime_t::attr_t attr;
  attr.rdv_protocol =
      rdv_protocol_.get_value_or(g_default_attr.runtime_attr.rdv_protocol);
  attr.use_reg_cache =
      use_reg_cache_.get_value_or(g_default_attr.runtime_attr.use_reg_cache);
  attr.use_control_channel = use_control_channel_.get_value_or(
      g_default_attr.runtime_attr.use_control_channel);
  attr.use_default_net_context = use_default_net_context_.get_value_or(
      g_default_attr.runtime_attr.use_default_net_context);
  attr.use_default_net_device = use_default_net_device_.get_value_or(
      g_default_attr.runtime_attr.use_default_net_device);
  runtime_->p_impl = new runtime_impl_t(attr);
  runtime_->p_impl->initialize();
}

void free_runtime_x::call() const
{
  delete runtime_->p_impl;
  runtime_->p_impl = nullptr;
  global_finalize();
}

void g_runtime_init_x::call() const
{
  global_initialize();
  LCIXX_Assert(g_default_runtime.p_impl == nullptr,
               "g_default_runtime has been initialized!\n");
  runtime_t::attr_t attr;
  attr.rdv_protocol =
      rdv_protocol_.get_value_or(g_default_attr.runtime_attr.rdv_protocol);
  attr.use_reg_cache =
      use_reg_cache_.get_value_or(g_default_attr.runtime_attr.use_reg_cache);
  attr.use_control_channel = use_control_channel_.get_value_or(
      g_default_attr.runtime_attr.use_control_channel);
  attr.use_default_net_context = use_default_net_context_.get_value_or(
      g_default_attr.runtime_attr.use_default_net_context);
  attr.use_default_net_device = use_default_net_device_.get_value_or(
      g_default_attr.runtime_attr.use_default_net_device);
  g_default_runtime.p_impl = new runtime_impl_t(attr);
  g_default_runtime.p_impl->initialize();
}

void g_runtime_fina_x::call() const
{
  delete g_default_runtime.p_impl;
  g_default_runtime.p_impl = nullptr;
  global_finalize();
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
  alloc_net_context_x(&net_context).runtime(runtime).call();
  alloc_net_device_x(&net_device).runtime(runtime).call();
  alloc_net_endpoint_x(&net_endpoint).runtime(runtime).call();
  alloc_packet_pool_x(&packet_pool).runtime(runtime).call();
  register_packets_x(packet_pool, net_device).runtime(runtime).call();

  if (net_context.get_attr().backend == option_backend_t::ofi &&
      net_context.get_attr().provider_name == "cxi") {
    // special setting for libfabric/cxi
    LCIXX_Assert(attr.use_reg_cache == false,
                 "The registration cache should be turned off "
                 "for libfabric cxi backend. Use `export LCIXX_USE_DREG=0`.\n");
    LCIXX_Assert(attr.use_control_channel == 0,
                 "The progress-specific network endpoint "
                 "for libfabric cxi backend. Use `export "
                 "LCIXX_ENABLE_PRG_NET_ENDPOINT=0`.\n");
    if (attr.rdv_protocol != option_rdv_protocol_t::write) {
      attr.rdv_protocol = option_rdv_protocol_t::write;
      LCIXX_Warn(
          "Switch LCIXX_RDV_PROTOCOL to \"write\" "
          "as required by the libfabric cxi backend\n");
    }
  }
}

runtime_impl_t::~runtime_impl_t()
{
  free_packet_pool_x(&packet_pool).runtime(runtime).call();
  free_net_endpoint_x(&net_endpoint).runtime(runtime).call();
  free_net_device_x(&net_device).runtime(runtime).call();
  free_net_context_x(&net_context).runtime(runtime).call();
}

void get_default_net_context_x::call() const
{
  *net_context_ = runtime_.get_value_or(g_default_runtime).p_impl->net_context;
}

void get_default_net_device_x::call() const
{
  *net_device_ = runtime_.get_value_or(g_default_runtime).p_impl->net_device;
}

void get_default_net_endpoint_x::call() const
{
  *net_endpoint_ =
      runtime_.get_value_or(g_default_runtime).p_impl->net_endpoint;
}

void get_default_packet_pool_x::call() const
{
  *packet_pool_ = runtime_.get_value_or(g_default_runtime).p_impl->packet_pool;
}

}  // namespace lcixx