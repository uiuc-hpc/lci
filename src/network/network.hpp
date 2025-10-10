// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

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
  virtual ~device_impl_t();
  virtual endpoint_t alloc_endpoint_impl(endpoint_t::attr_t attr) = 0;
  virtual mr_t register_memory_impl(void* buffer, size_t size) = 0;
  virtual void deregister_memory_impl(mr_impl_t* mr) = 0;
  virtual uint64_t get_rkey(mr_impl_t* mr) = 0;
  virtual size_t poll_comp_impl(net_status_t* p_statuses, size_t max_polls) = 0;
  virtual error_t post_recv_impl(void* buffer, size_t size, mr_t mr,
                                 void* user_context) = 0;
  virtual size_t post_recvs_impl(void* buffers[], size_t size, size_t count,
                                 mr_t mr, void* usesr_contexts[]) = 0;

  // wrapper functions
  endpoint_t alloc_endpoint(endpoint_t::attr_t attr);
  void free_endpoint(endpoint_t endpoint);
  inline mr_t register_memory(void* buffer, size_t size);
  inline void deregister_memory(mr_impl_t* mr);
  inline size_t poll_comp(net_status_t* p_statuses, size_t max_polls);
  inline error_t post_recv(void* buffer, size_t size, mr_t mr,
                           void* user_context);
  inline size_t post_recvs(void* buffers[], size_t size, size_t count, mr_t mr,
                           void* usesr_contexts[]);

  // LCI layer functions
  inline void bind_packet_pool(packet_pool_t packet_pool_);
  inline void unbind_packet_pool();
  inline bool post_recv_packets();
  inline bool refill_recvs(bool is_blocking = false);
  inline void consume_recvs(int n) { nrecvs_posted -= n; }
  static int reserve_device_ids(int n)
  {
    return g_ndevices.fetch_add(n, std::memory_order_relaxed);
  }

  attr_t attr;
  endpoint_t default_endpoint;
  device_t device;
  runtime_t runtime;
  net_context_t net_context;
  mpmc_array_t<endpoint_t> endpoints;
  std::atomic<int> next_endpoint_idx;
  packet_pool_t packet_pool;

  LCIU_CACHE_PADDING(sizeof(packet_pool_t));

 private:
  static std::atomic<int> g_ndevices;
  LCIU_CACHE_PADDING(sizeof(std::atomic<int>));
  std::atomic<size_t> nrecvs_posted;
  LCIU_CACHE_PADDING(sizeof(std::atomic<size_t>));
};

class mr_impl_t
{
 public:
  mr_attr_t attr;
  device_t device;
  void* address;
  size_t size;
#ifdef LCI_USE_CUDA
  accelerator::buffer_attr_t acc_attr;
  int dmabuf_fd;
#endif  // LCI_USE_CUDA
  // TODO: add memory registration cache
  // For memory registration cache.
  // void* region;

  // convenience functions
  inline void deregister();
  inline rmr_t get_rmr();
};

class endpoint_impl_t
{
 public:
  using attr_t = endpoint_t::attr_t;

  // functions for backends to implement
  endpoint_impl_t(device_t device_, attr_t attr_);
  virtual ~endpoint_impl_t() = default;
  virtual error_t post_sends_impl(int rank, void* buffer, size_t size,
                                  net_imm_data_t imm_data,
                                  void* user_context) = 0;
  virtual error_t post_send_impl(int rank, void* buffer, size_t size, mr_t mr,
                                 net_imm_data_t imm_data,
                                 void* user_context) = 0;
  virtual error_t post_puts_impl(int rank, void* buffer, size_t size,
                                 uint64_t offset, rmr_t rmr,
                                 void* user_context) = 0;
  virtual error_t post_put_impl(int rank, void* buffer, size_t size, mr_t mr,
                                uint64_t offset, rmr_t rmr,
                                void* user_context) = 0;
  virtual error_t post_putImms_impl(int rank, void* buffer, size_t size,
                                    uint64_t offset, rmr_t rmr,
                                    net_imm_data_t imm_data,
                                    void* user_context) = 0;
  virtual error_t post_putImm_impl(int rank, void* buffer, size_t size, mr_t mr,
                                   uint64_t offset, rmr_t rmr,
                                   net_imm_data_t imm_data,
                                   void* user_context) = 0;
  virtual error_t post_get_impl(int rank, void* buffer, size_t size, mr_t mr,
                                uint64_t offset, rmr_t rmr,
                                void* user_context) = 0;

  // wrapper functions
  inline error_t post_sends(int rank, void* buffer, size_t size,
                            net_imm_data_t imm_data, void* user_context,
                            bool allow_retry = true, bool force_post = false);
  inline error_t post_send(int rank, void* buffer, size_t size, mr_t mr,
                           net_imm_data_t imm_data, void* user_context,
                           bool allow_retry = true, bool force_post = false);
  inline error_t post_puts(int rank, void* buffer, size_t size, uint64_t offset,
                           rmr_t rmr, void* user_context,
                           bool allow_retry = true, bool force_post = false);
  inline error_t post_put(int rank, void* buffer, size_t size, mr_t mr,
                          uint64_t offset, rmr_t rmr, void* user_context,
                          bool allow_retry = true, bool force_post = false);
  inline error_t post_putImms(int rank, void* buffer, size_t size,
                              uint64_t offset, rmr_t rmr,
                              net_imm_data_t imm_data, void* user_context,
                              bool allow_retry = true, bool force_post = false);
  inline error_t post_putImm(int rank, void* buffer, size_t size, mr_t mr,
                             uint64_t offset, rmr_t rmr,
                             net_imm_data_t imm_data, void* user_context,
                             bool allow_retry = true, bool force_post = false);
  inline error_t post_get(int rank, void* buffer, size_t size, mr_t mr,
                          uint64_t offset, rmr_t rmr, void* user_context,
                          bool allow_retry = true, bool force_post = false);
  inline error_t post_putImms_fallback(int rank, void* buffer, size_t size,
                                       uint64_t offset, rmr_t rmr,
                                       net_imm_data_t imm_data,
                                       void* user_context);
  inline error_t post_putImm_fallback(int rank, void* buffer, size_t size,
                                      mr_t mr, uint64_t offset, rmr_t rmr,
                                      net_imm_data_t imm_data,
                                      void* user_context);
  inline bool progress_backlog_queue() { return backlog_queue.progress(); }
  inline bool is_backlog_queue_empty() const
  {
    return backlog_queue.is_empty();
  }
  inline bool is_backlog_queue_empty(int rank) const
  {
    return backlog_queue.is_empty(rank);
  }

  inline void add_pending_ops(int64_t n = 1)
  {
    [[maybe_unused]] int64_t ret =
        pending_ops.fetch_add(n, std::memory_order_relaxed);
    // fprintf(stderr, "add pending ops %ld + %ld = %ld\n", ret, n, ret + n);
  }
  inline void sub_pending_ops(int64_t n = 1)
  {
    [[maybe_unused]] int64_t ret =
        pending_ops.fetch_sub(n, std::memory_order_relaxed);
    LCI_DBG_Assert(ret - n >= 0,
                   "pending_ops is negative! (n: %ld, pending_ops: %ld)\n", n,
                   ret - n);
    // fprintf(stderr, "sub pending ops %ld - %ld = %ld\n", ret, n, ret - n);
  }
  inline int64_t get_pending_ops() const
  {
    return pending_ops.load(std::memory_order_relaxed);
  }

  runtime_t runtime;
  device_t device;
  endpoint_t endpoint;
  attr_t attr;
  net_context_attr_t net_context_attr;
  int idx_in_device;

 private:
  static std::atomic<int> g_nendpoints;
  backlog_queue_t backlog_queue;
  LCIU_CACHE_PADDING(sizeof(backlog_queue_t));
  std::atomic<int64_t> pending_ops;
  LCIU_CACHE_PADDING(sizeof(std::atomic<int64_t>));
};

}  // namespace lci

#endif  // LCI_NETWORK_HPP
