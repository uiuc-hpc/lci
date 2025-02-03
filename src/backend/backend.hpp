#ifndef LCI_BACKEND_BACKEND_HPP
#define LCI_BACKEND_BACKEND_HPP

namespace lci
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
      : context(context_), attr(attr_), nrecvs_posted(0)
  {
    net_device_id = g_ndevices++;
    runtime = context.p_impl->runtime;
    net_device.p_impl = this;
  };
  virtual ~net_device_impl_t();
  virtual net_endpoint_t alloc_net_endpoint(net_endpoint_t::attr_t attr) = 0;
  virtual mr_t register_memory(void* buffer, size_t size) = 0;
  virtual void deregister_memory(mr_t) = 0;
  virtual rkey_t get_rkey(mr_t mr) = 0;
  virtual std::vector<net_status_t> poll_comp_impl(int max_polls) = 0;
  inline std::vector<net_status_t> poll_comp(int max_polls)
  {
    auto statuses = poll_comp_impl(max_polls);
    LCI_PCOUNTER_ADD(net_poll_cq_entry_count, statuses.size());
    return statuses;
  }
  virtual error_t post_recv_impl(void* buffer, size_t size, mr_t mr,
                                 void* ctx) = 0;
  inline error_t post_recv(void* buffer, size_t size, mr_t mr, void* ctx)
  {
    error_t error = post_recv_impl(buffer, size, mr, ctx);
    if (error.is_retry()) {
      LCI_PCOUNTER_ADD(net_recv_post_retry, 1);
    } else {
      LCI_PCOUNTER_ADD(net_recv_post, 1);
    }
    return error;
  }
  bool post_recv_packet();
  void refill_recvs(bool is_blocking = false);
  void consume_recvs(int n) { nrecvs_posted -= n; }
  void bind_packet_pool(packet_pool_t packet_pool_);
  void unbind_packet_pool();

  runtime_t runtime;
  net_device_t net_device;
  net_context_t context;
  attr_t attr;
  int net_device_id;
  packet_pool_t packet_pool;
  std::atomic<int> nrecvs_posted;
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

  virtual error_t post_sends_impl(int rank, void* buffer, size_t size,
                                  net_imm_data_t imm_data) = 0;
  virtual error_t post_send_impl(int rank, void* buffer, size_t size, mr_t mr,
                                 net_imm_data_t imm_data, void* ctx) = 0;
  virtual error_t post_puts_impl(int rank, void* buffer, size_t size,
                                 uintptr_t base, uint64_t offset,
                                 rkey_t rkey) = 0;
  virtual error_t post_put_impl(int rank, void* buffer, size_t size, mr_t mr,
                                uintptr_t base, uint64_t offset, rkey_t rkey,
                                void* ctx) = 0;
  virtual error_t post_putImms_impl(int rank, void* buffer, size_t size,
                                    uintptr_t base, uint64_t offset,
                                    rkey_t rkey, net_imm_data_t imm_data) = 0;
  virtual error_t post_putImm_impl(int rank, void* buffer, size_t size, mr_t mr,
                                   uintptr_t base, uint64_t offset, rkey_t rkey,
                                   net_imm_data_t imm_data, void* ctx) = 0;

  inline error_t post_sends(int rank, void* buffer, size_t size,
                            net_imm_data_t imm_data)
  {
    auto error = post_sends_impl(rank, buffer, size, imm_data);
    if (error.is_retry()) {
      LCI_PCOUNTER_ADD(net_send_post_retry, 1);
    } else {
      LCI_PCOUNTER_ADD(net_send_post, 1);
    }
    return error;
  }

  inline error_t post_send(int rank, void* buffer, size_t size, mr_t mr,
                           net_imm_data_t imm_data, void* ctx)
  {
    auto error = post_send_impl(rank, buffer, size, mr, imm_data, ctx);
    if (error.is_retry()) {
      LCI_PCOUNTER_ADD(net_send_post_retry, 1);
    } else {
      LCI_PCOUNTER_ADD(net_send_post, 1);
    }
    return error;
  }

  inline error_t post_puts(int rank, void* buffer, size_t size, uintptr_t base,
                           uint64_t offset, rkey_t rkey)
  {
    auto error = post_puts_impl(rank, buffer, size, base, offset, rkey);
    if (error.is_retry()) {
      LCI_PCOUNTER_ADD(net_send_post_retry, 1);
    } else {
      LCI_PCOUNTER_ADD(net_send_post, 1);
    }
    return error;
  }

  inline error_t post_put(int rank, void* buffer, size_t size, mr_t mr,
                          uintptr_t base, uint64_t offset, rkey_t rkey,
                          void* ctx)
  {
    auto error = post_put_impl(rank, buffer, size, mr, base, offset, rkey, ctx);
    if (error.is_retry()) {
      LCI_PCOUNTER_ADD(net_send_post_retry, 1);
    } else {
      LCI_PCOUNTER_ADD(net_send_post, 1);
    }
    return error;
  }

  inline error_t post_putImms(int rank, void* buffer, size_t size,
                              uintptr_t base, uint64_t offset, rkey_t rkey,
                              net_imm_data_t imm_data)
  {
    auto error =
        post_putImms_impl(rank, buffer, size, base, offset, rkey, imm_data);
    if (error.is_retry()) {
      LCI_PCOUNTER_ADD(net_send_post_retry, 1);
    } else {
      LCI_PCOUNTER_ADD(net_send_post, 1);
    }
    return error;
  }

  inline error_t post_putImm(int rank, void* buffer, size_t size, mr_t mr,
                             uintptr_t base, uint64_t offset, rkey_t rkey,
                             net_imm_data_t imm_data, void* ctx)
  {
    auto error = post_putImm_impl(rank, buffer, size, mr, base, offset, rkey,
                                  imm_data, ctx);
    if (error.is_retry()) {
      LCI_PCOUNTER_ADD(net_send_post_retry, 1);
    } else {
      LCI_PCOUNTER_ADD(net_send_post, 1);
    }
    return error;
  }

  runtime_t runtime;
  net_device_t net_device;
  net_endpoint_t net_endpoint;
  attr_t attr;
  int net_endpoint_id;
};

}  // namespace lci

#endif  // LCI_BACKEND_BACKEND_HPP