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
  attr.lock_mode =
      lock_mode_.get_value_or(g_default_attr.net_device_attr.lock_mode);
  *net_device_ = context.p_impl->alloc_net_device(attr);
}

void free_net_device_x::call() const
{
  delete net_device_->p_impl;
  net_device_->p_impl = nullptr;
}

void register_memory_x::call() const
{
  runtime_t runtime = runtime_.get_value_or(g_default_runtime);
  net_device_t net_device;
  if (!net_device_.get_value(&net_device)) {
    get_default_net_device_x(&net_device).runtime(runtime).call();
  }
  *mr_ = net_device.p_impl->register_memory(address_, size_);
  mr_->p_impl->device = net_device;
}

void deregister_memory_x::call() const
{
  mr_->p_impl->device.p_impl->deregister_memory(*mr_);
  mr_->p_impl = nullptr;
}

void net_poll_cq_x::call() const
{
  runtime_t runtime = runtime_.get_value_or(g_default_runtime);
  net_device_t net_device;
  if (!net_device_.get_value(&net_device)) {
    get_default_net_device_x(&net_device).runtime(runtime).call();
  }
  const int default_max_polls = 20;
  int max_polls = max_polls_.get_value_or(default_max_polls);
  *statuses_ = net_device.p_impl->poll_comp(max_polls);
}

std::atomic<int> net_endpoint_impl_t::g_nendpoints(0);

void alloc_net_endpoint_x::call() const
{
  runtime_t runtime = runtime_.get_value_or(g_default_runtime);
  net_device_t device;
  if (!net_device_.get_value(&device)) {
    get_default_net_device_x(&device).runtime(runtime).call();
  }
  net_endpoint_t::attr_t attr;
  *net_endpoint_ = device.p_impl->alloc_net_endpoint(attr);
}

void free_net_endpoint_x::call() const
{
  delete net_endpoint_->p_impl;
  net_endpoint_->p_impl = nullptr;
}

void net_post_recv_x::call() const
{
  runtime_t runtime = runtime_.get_value_or(g_default_runtime);
  net_device_t net_device;
  if (!net_device_.get_value(&net_device)) {
    get_default_net_device_x(&net_device).runtime(runtime).call();
  }
  void* ctx = ctx_.get_value_or(nullptr);
  *error_ = net_device.p_impl->post_recv(buffer_, size_, mr_, ctx);
}

void net_post_sends_x::call() const
{
  runtime_t runtime = runtime_.get_value_or(g_default_runtime);
  net_endpoint_t net_endpoint;
  if (!net_endpoint_.get_value(&net_endpoint)) {
    get_default_net_endpoint_x(&net_endpoint).runtime(runtime).call();
  }
  net_imm_data_t imm_data = imm_data_.get_value_or(0);
  *error_ = net_endpoint.p_impl->post_sends(rank_, buffer_, size_, imm_data);
}

void net_post_send_x::call() const
{
  runtime_t runtime = runtime_.get_value_or(g_default_runtime);
  net_endpoint_t net_endpoint;
  if (!net_endpoint_.get_value(&net_endpoint)) {
    get_default_net_endpoint_x(&net_endpoint).runtime(runtime).call();
  }
  void* ctx = ctx_.get_value_or(nullptr);
  net_imm_data_t imm_data = imm_data_.get_value_or(0);
  *error_ =
      net_endpoint.p_impl->post_send(rank_, buffer_, size_, mr_, imm_data, ctx);
}

void net_post_puts_x::call() const
{
  runtime_t runtime = runtime_.get_value_or(g_default_runtime);
  net_endpoint_t net_endpoint;
  if (!net_endpoint_.get_value(&net_endpoint)) {
    get_default_net_endpoint_x(&net_endpoint).runtime(runtime).call();
  }
  *error_ = net_endpoint.p_impl->post_puts(rank_, buffer_, size_, base_,
                                           offset_, rkey_);
}

void net_post_put_x::call() const
{
  runtime_t runtime = runtime_.get_value_or(g_default_runtime);
  net_endpoint_t net_endpoint;
  if (!net_endpoint_.get_value(&net_endpoint)) {
    get_default_net_endpoint_x(&net_endpoint).runtime(runtime).call();
  }
  void* ctx = ctx_.get_value_or(nullptr);
  *error_ = net_endpoint.p_impl->post_put(rank_, buffer_, size_, mr_, base_,
                                          offset_, rkey_, ctx);
}

void net_post_putImms_x::call() const
{
  runtime_t runtime = runtime_.get_value_or(g_default_runtime);
  net_endpoint_t net_endpoint;
  if (!net_endpoint_.get_value(&net_endpoint)) {
    get_default_net_endpoint_x(&net_endpoint).runtime(runtime).call();
  }
  net_imm_data_t imm_data = imm_data_.get_value_or(0);
  *error_ = net_endpoint.p_impl->post_putImms(rank_, buffer_, size_, base_,
                                              offset_, rkey_, imm_data);
}

void net_post_putImm_x::call() const
{
  runtime_t runtime = runtime_.get_value_or(g_default_runtime);
  net_endpoint_t net_endpoint;
  if (!net_endpoint_.get_value(&net_endpoint)) {
    get_default_net_endpoint_x(&net_endpoint).runtime(runtime).call();
  }
  void* ctx = ctx_.get_value_or(nullptr);
  net_imm_data_t imm_data = imm_data_.get_value_or(0);
  *error_ = net_endpoint.p_impl->post_putImm(rank_, buffer_, size_, mr_, base_,
                                             offset_, rkey_, imm_data, ctx);
}
}  // namespace lcixx