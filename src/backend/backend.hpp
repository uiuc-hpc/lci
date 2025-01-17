#ifndef LCIXX_BACKEND_BACKEND_HPP
#define LCIXX_BACKEND_BACKEND_HPP

namespace lcixx
{
class net_context_impl_t
{
 public:
  net_context_impl_t(runtime_t runtime_, net_context_t::attr_t attr_)
      : runtime(runtime_), attr(attr_){};
  virtual ~net_context_impl_t() = default;
  virtual net_device_t alloc_net_device(net_device_t::attr_t attr) = 0;
  net_context_t get_handler()
  {
    net_context_t ret;
    ret.p_impl = this;
    return ret;
  }

  runtime_t runtime;
  net_context_t::attr_t attr;
};

class net_device_impl_t
{
 public:
  static std::atomic<int> g_ndevices;

  using attr_t = net_device_t::attr_t;

  net_device_impl_t(net_context_t context_, attr_t attr_)
      : context(context_), attr(attr_)
  {
    net_device_id = g_ndevices++;
    runtime = context.p_impl->runtime;
  };
  virtual ~net_device_impl_t() = default;
  virtual mr_t register_memory(void* address, size_t size) = 0;
  virtual void deregister_memory(mr_t) = 0;
  virtual rkey_t get_rkey(mr_t mr) = 0;
  virtual std::vector<net_status_t> poll_comp(int max_polls) = 0;

  runtime_t runtime;
  net_context_t context;
  attr_t attr;
  int net_device_id;
};

class mr_impl_t
{
 public:
  using attr_t = mr_t::attr_t;
  attr_t attr;
  net_device_t device;
  // TODO: add memory registration cache
  // For memory registration cache.
  // void* region;
};

class net_endpoint_impl_t
{
 public:
  static std::atomic<int> g_nendpoints;

  using attr_t = net_endpoint_t::attr_t;

  net_endpoint_impl_t(net_device_t device_, attr_t attr_)
      : runtime(device_.p_impl->runtime), device(device_), attr(attr_)
  {
    net_endpoint_id = g_nendpoints++;
  }
  virtual ~net_endpoint_impl_t() = default;
  net_endpoint_t get_handler()
  {
    net_endpoint_t ret;
    ret.p_impl = this;
    return ret;
  }

  runtime_t runtime;
  net_device_t device;
  attr_t attr;
  int net_endpoint_id;
};

}  // namespace lcixx

#endif  // LCIXX_BACKEND_BACKEND_HPP