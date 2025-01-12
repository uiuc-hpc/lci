#include "lcixx_internal.hpp"

namespace lcixx
{
net_context_t alloc_net_context_x::call()
{
  net_context_t ret;
  net_context_t::config_t config;
  option_backend_t backend_to_use =
      backend.get_value_or(g_default_config.backend);
  switch (backend_to_use) {
    case option_backend_t::none:
      break;
    case option_backend_t::ibv:
#ifdef LCIXX_BACKEND_ENABLE_IBV
      // ret.p_impl = new ibv_net_context_impl_t(runtime, config);
#else
      LCIXX_Assert(false, "IBV backend is not enabled\n");
#endif
      break;
    case option_backend_t::ofi:
#ifdef LCIXX_BACKEND_ENABLE_OFI
      ret.p_impl = new ofi_net_context_impl_t(runtime, config);
#else
      LCIXX_Assert(false, "OFI backend is not enabled\n");
#endif
      break;
    case option_backend_t::ucx:
#ifdef LCIXX_BACKEND_ENABLE_UCX
      // ret.p_impl = new ucx_net_context_impl_t(runtime, config);
#else
      LCIXX_Assert(false, "UCX backend is not enabled\n");
#endif
      break;
    default:
      LCIXX_Assert(false, "Unsupported backend %d\n", backend_to_use);
  }
  return ret;
}

void free_net_context_x::call() { delete net_context.p_impl; }

net_context_t::config_t net_context_t::get_config() const
{
  return p_impl->config;
}

std::atomic<int> net_device_impl_t::g_ndevices(0);

net_device_t alloc_net_device_x::call()
{
  net_device_t ret;
  net_context_t context_obj =
      context.get_value_or(runtime.get_default_net_context());
  net_device_t::config_t config;
  config.max_sends = max_sends.get_value_or(g_default_config.backend_max_sends);
  config.max_recvs = max_recvs.get_value_or(g_default_config.backend_max_recvs);
  config.max_cqes = max_cqes.get_value_or(g_default_config.backend_max_cqes);
  ret.p_impl = new ofi_net_device_impl_t(context_obj, config);
  return ret;
}

void free_net_device_x::call() { delete device.p_impl; }

mr_t register_memory_x::call()
{
  mr_t mr = device.p_impl->register_memory(address, size);
  mr.p_impl->device = device;
  return mr;
}

void deregister_memory_x::call()
{
  mr.p_impl->device.p_impl->deregister_memory(mr);
}

}  // namespace lcixx