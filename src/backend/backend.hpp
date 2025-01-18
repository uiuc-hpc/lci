#ifndef LCIXX_BACKEND_BACKEND_HPP
#define LCIXX_BACKEND_BACKEND_HPP

namespace lcixx
{
class net_context_impl_t
{
 public:
  net_context_impl_t(runtime_t runtime_, net_context_t::attr_t attr_)
      : runtime(runtime_), attr(attr_)
  {
    net_context.p_impl = this;
  };
  virtual ~net_context_impl_t() = default;
  virtual net_device_t alloc_net_device(net_device_t::attr_t attr) = 0;

  runtime_t runtime;
  net_context_t net_context;
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
    net_device.p_impl = this;
  };
  virtual ~net_device_impl_t() = default;
  virtual net_endpoint_t alloc_net_endpoint(net_endpoint_t::attr_t attr) = 0;
  virtual mr_t register_memory(void* buffer, size_t size) = 0;
  virtual void deregister_memory(mr_t) = 0;
  virtual rkey_t get_rkey(mr_t mr) = 0;
  virtual std::vector<net_status_t> poll_comp(int max_polls) = 0;
  virtual error_t post_recv(void* buffer, size_t size, mr_t mr, void* ctx) = 0;

  runtime_t runtime;
  net_device_t net_device;
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

  net_endpoint_impl_t(net_device_t net_device_, attr_t attr_)
      : runtime(net_device_.p_impl->runtime),
        net_device(net_device_),
        attr(attr_)
  {
    net_endpoint_id = g_nendpoints++;
    net_endpoint.p_impl = this;
  }
  virtual ~net_endpoint_impl_t() = default;
  virtual error_t post_sends(int rank, void* buffer, size_t size,
                             net_imm_data_t imm_data) = 0;
  virtual error_t post_send(int rank, void* buffer, size_t size, mr_t mr,
                            net_imm_data_t imm_data, void* ctx) = 0;
  virtual error_t post_puts(int rank, void* buffer, size_t size, uintptr_t base,
                            uint64_t offset, rkey_t rkey) = 0;
  virtual error_t post_put(int rank, void* buffer, size_t size, mr_t mr,
                           uintptr_t base, uint64_t offset, rkey_t rkey,
                           void* ctx) = 0;
  virtual error_t post_putImms(int rank, void* buffer, size_t size,
                               uintptr_t base, uint64_t offset, rkey_t rkey,
                               net_imm_data_t imm_data) = 0;
  virtual error_t post_putImm(int rank, void* buffer, size_t size, mr_t mr,
                              uintptr_t base, uint64_t offset, rkey_t rkey,
                              net_imm_data_t imm_data, void* ctx) = 0;

  runtime_t runtime;
  net_device_t net_device;
  net_endpoint_t net_endpoint;
  attr_t attr;
  int net_endpoint_id;
};

}  // namespace lcixx

#endif  // LCIXX_BACKEND_BACKEND_HPP