#include "lcixx_internal.hpp"

namespace lcixx
{
void alloc_net_context_x::call() const
{
  runtime_t runtime = runtime_.get_value_or(g_default_runtime);
  net_context_t::attr_t attr;
  attr.backend = backend_.get_value_or(g_default_attr.net_context_attr.backend);
  attr.provider_name = provider_name_.get_value_or(
      g_default_attr.net_context_attr.provider_name);
  attr.max_msg_size =
      max_msg_size_.get_value_or(g_default_attr.net_context_attr.max_msg_size);

  switch (attr.backend) {
    case option_backend_t::none:
      break;
    case option_backend_t::ibv:
#ifdef LCIXX_BACKEND_ENABLE_IBV
      // ret.p_impl = new ibv_net_context_impl_t(runtime, attr);
#else
      LCIXX_Assert(false, "IBV backend is not enabled\n");
#endif
      break;
    case option_backend_t::ofi:
#ifdef LCIXX_BACKEND_ENABLE_OFI
      net_context_->p_impl = new ofi_net_context_impl_t(runtime, attr);
#else
      LCIXX_Assert(false, "OFI backend is not enabled\n");
#endif
      break;
    case option_backend_t::ucx:
#ifdef LCIXX_BACKEND_ENABLE_UCX
      // ret.p_impl = new ucx_net_context_impl_t(runtime, attr);
#else
      LCIXX_Assert(false, "UCX backend is not enabled\n");
#endif
      break;
    default:
      LCIXX_Assert(false, "Unsupported backend %d\n", attr.backend);
  }
}

void free_net_context_x::call() const
{
  delete net_context_->p_impl;
  net_context_->p_impl = nullptr;
}

std::atomic<int> net_device_impl_t::g_ndevices(0);

void alloc_net_device_x::call() const
{
  runtime_t runtime = runtime_.get_value_or(g_default_runtime);
  net_context_t context;
  if (!net_context_.get_value(&context)) {
    get_default_net_context_x(&context).runtime(runtime).call();
  }
  net_device_t::attr_t attr;
  attr.max_sends =
      max_sends_.get_value_or(g_default_attr.net_device_attr.max_sends);
  attr.max_recvs =
      max_recvs_.get_value_or(g_default_attr.net_device_attr.max_recvs);
  attr.max_cqes =
      max_cqes_.get_value_or(g_default_attr.net_device_attr.max_cqes);
  net_device_->p_impl = new ofi_net_device_impl_t(context, attr);
}

void free_net_device_x::call() const
{
  delete net_device_->p_impl;
  net_device_->p_impl = nullptr;
}

void register_memory_x::call() const
{
  *mr_ = device_.p_impl->register_memory(address_, size_);
  mr_->p_impl->device = device_;
}

void deregister_memory_x::call() const
{
  mr_.p_impl->device.p_impl->deregister_memory(mr_);
}

void net_poll_cq_x::call() const
{
  *statuses_ = device_.p_impl->poll_comp(max_polls_.get_value_or(1));
}

std::atomic<int> net_endpoint_impl_t::g_nendpoints(0);

}  // namespace lcixx