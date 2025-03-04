#ifndef LCI_NETWORK_HPP
#define LCI_NETWORK_HPP

namespace lci
{
class net_context_impl_t
{
 public:
  using attr_t = net_context_t::attr_t;

  // functions for backends to implement
  net_context_impl_t(runtime_t runtime_, attr_t attr_);
  virtual ~net_context_impl_t() = default;
  virtual device_t alloc_device(device_t::attr_t attr) = 0;

  attr_t attr;
  net_context_t net_context;
  runtime_t runtime;
};

class device_impl_t
{
 public:
  using attr_t = device_t::attr_t;

  // functions for backends to implement
  device_impl_t(net_context_t context_, attr_t attr_);
  virtual ~device_impl_t() = default;
  virtual endpoint_t alloc_endpoint(endpoint_t::attr_t attr) = 0;
  virtual mr_t register_memory_impl(void* buffer, size_t size) = 0;
  virtual void deregister_memory_impl(mr_impl_t* mr) = 0;
  virtual rkey_t get_rkey(mr_impl_t* mr) = 0;
  virtual std::vector<net_status_t> poll_comp_impl(int max_polls) = 0;
  virtual error_t post_recv_impl(void* buffer, size_t size, mr_t mr,
                                 void* ctx) = 0;

  // wrapper functions
  inline mr_t register_memory(void* buffer, size_t size);
  inline void deregister_memory(mr_impl_t* mr);
  inline std::vector<net_status_t> poll_comp(int max_polls);
  inline error_t post_recv(void* buffer, size_t size, mr_t mr, void* ctx);

  // LCI layer functions
  inline void bind_packet_pool(packet_pool_t packet_pool_);
  inline void unbind_packet_pool();
  inline bool post_recv_packet();
  inline void refill_recvs(bool is_blocking = false);
  inline void consume_recvs(int n) { nrecvs_posted -= n; }

  attr_t attr;
  device_t device;
  runtime_t runtime;
  net_context_t net_context;
  packet_pool_t packet_pool;

  static std::atomic<int> g_ndevices;

  int device_id;
  std::atomic<int> nrecvs_posted;
};

class mr_impl_t
{
 public:
  device_t device;
  void* address;
  size_t size;
  // TODO: add memory registration cache
  // For memory registration cache.
  // void* region;

  // convenience functions
  inline void deregister();
  inline rkey_t get_rkey();
};

class endpoint_impl_t
{
 public:
  using attr_t = endpoint_t::attr_t;

  // functions for backends to implement
  endpoint_impl_t(device_t device_, attr_t attr_);
  virtual ~endpoint_impl_t() = default;
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
  virtual error_t post_get_impl(int rank, void* buffer, size_t size, mr_t mr,
                                uintptr_t base, uint64_t offset, rkey_t rkey,
                                void* ctx) = 0;

  // wrapper functions
  inline error_t post_sends(int rank, void* buffer, size_t size,
                            net_imm_data_t imm_data);
  inline error_t post_send(int rank, void* buffer, size_t size, mr_t mr,
                           net_imm_data_t imm_data, void* ctx);
  inline error_t post_puts(int rank, void* buffer, size_t size, uintptr_t base,
                           uint64_t offset, rkey_t rkey);
  inline error_t post_put(int rank, void* buffer, size_t size, mr_t mr,
                          uintptr_t base, uint64_t offset, rkey_t rkey,
                          void* ctx);
  inline error_t post_putImms(int rank, void* buffer, size_t size,
                              uintptr_t base, uint64_t offset, rkey_t rkey,
                              net_imm_data_t imm_data);
  inline error_t post_putImm(int rank, void* buffer, size_t size, mr_t mr,
                             uintptr_t base, uint64_t offset, rkey_t rkey,
                             net_imm_data_t imm_data, void* ctx);
  inline error_t post_get(int rank, void* buffer, size_t size, mr_t mr,
                          uintptr_t base, uint64_t offset, rkey_t rkey,
                          void* ctx);

  runtime_t runtime;
  device_t device;
  endpoint_t endpoint;
  attr_t attr;

  static std::atomic<int> g_nendpoints;
  int endpoint_id;
};

}  // namespace lci

#endif  // LCI_NETWORK_HPP