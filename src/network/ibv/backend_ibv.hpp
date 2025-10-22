// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#ifndef LCI_BACKEND_IBV_BACKEND_IBV_HPP
#define LCI_BACKEND_IBV_BACKEND_IBV_HPP

#include "infiniband/verbs.h"
#include <atomic>
#include <algorithm>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <vector>

#define IBV_SAFECALL(x)                                                        \
  do {                                                                         \
    int err = (x);                                                             \
    if (err) {                                                                 \
      LCI_Assert(false, "err %d : %s (%s:%d)\n", err, strerror(err), __FILE__, \
                 __LINE__);                                                    \
    }                                                                          \
  } while (0)

#define IBV_SAFECALL_RET(x)                                                    \
  do {                                                                         \
    int err = (x);                                                             \
    if (err) {                                                                 \
      LCI_Assert(false, "err %d : %s (%s:%d)\n", err, strerror(err), __FILE__, \
                 __LINE__);                                                    \
    }                                                                          \
    return errorcode_t::fatal;                                                 \
  } while (0)

namespace lci
{
class ibv_net_context_impl_t : public lci::net_context_impl_t
{
 public:
  static bool check_availability();

  ibv_net_context_impl_t(runtime_t runtime_, attr_t attr_);
  ~ibv_net_context_impl_t() override;
  device_t alloc_device(device_t::attr_t attr) override;

  struct ibv_device** ib_dev_list;
  struct ibv_device* ib_dev;
  struct ibv_context* ib_context;
  struct ibv_pd* ib_pd;
  struct ibv_device_attr ib_dev_attr;
  struct ibv_device_attr_ex ib_dev_attrx;
  struct ibv_port_attr ib_port_attr;
  uint8_t ib_dev_port;
  struct ibv_mr* ib_odp_mr;
  size_t max_inline;
  union ibv_gid ib_gid;
};

struct LCISI_ibv_qp_extra_t {
  struct ibv_td* ib_td;
  struct ibv_pd* ib_pd;
  LCIU_CACHE_PADDING(sizeof(struct ibv_td*) + sizeof(struct ibv_pd*));
  spinlock_t lock;
  LCIU_CACHE_PADDING(sizeof(spinlock_t));
};

class ibv_mr_impl_t : public lci::mr_impl_t
{
 public:
  struct ibv_mr* ibv_mr;
};

class qp2rank_map_t
{
 public:
  struct entry_t {
    int rank = -1;
    padded_atomic_t<int>* slots = nullptr;
  };
  
  struct snapshot_t {
    int mod = 1;
    std::vector<entry_t> table;

    entry_t get_entry(uint32_t qp_num);
  };

  ~qp2rank_map_t() { 
    if (current_snapshot) {
      delete current_snapshot;
    }
  }

  void add_qps(const std::vector<struct ibv_qp*>& qps,
               std::vector<padded_atomic_t<int>>* qp_slots);
  void remove_qps(const std::vector<struct ibv_qp*>& qps);
  snapshot_t *get_snapshot()
  {
    return current_snapshot.load(std::memory_order_relaxed);
  }

 private:

  void rebuild_locked();

  std::vector<std::pair<uint32_t, entry_t>> qp_entries;
  std::atomic<snapshot_t*> current_snapshot = nullptr;
  mutable std::shared_mutex mutex;
};

class ibv_device_impl_t : public lci::device_impl_t
{
 public:
  ibv_device_impl_t(net_context_t context_, device_t::attr_t attr_);
  ~ibv_device_impl_t() override;
  endpoint_t alloc_endpoint_impl(endpoint_t::attr_t attr) override;
  mr_t register_memory_impl(void* buffer, size_t size) override;
  void deregister_memory_impl(mr_impl_t*) override;
  uint64_t get_rkey(mr_impl_t* mr) override;
  size_t poll_comp_impl(net_status_t* p_statuses, size_t max_polls) override;
  error_t post_recv_impl(void* buffer, size_t size, mr_t mr,
                         void* user_context) override;
  size_t post_recvs_impl(void* buffers[], size_t size, size_t count, mr_t mrm,
                         void* usesr_contexts[]) override;

  // Connections O(N)
  struct ibv_td* ib_td;
  struct ibv_pd* ib_pd;
  struct ibv_cq* ib_cq;
  struct ibv_srq* ib_srq;
  qp2rank_map_t qp2rank_map;

  ibv_mr_impl_t odp_mr;
  LCIU_CACHE_PADDING(0);
  spinlock_t srq_lock;
  LCIU_CACHE_PADDING(sizeof(spinlock_t));
  spinlock_t cq_lock;
  LCIU_CACHE_PADDING(sizeof(spinlock_t));
};

class ibv_endpoint_impl_t : public lci::endpoint_impl_t
{
 public:
  ibv_endpoint_impl_t(device_t device_, attr_t attr_);
  ~ibv_endpoint_impl_t() override;
  error_t post_sends_impl(int rank, void* buffer, size_t size,
                          net_imm_data_t imm_data, void* user_context,
                          bool high_priority) override;
  error_t post_send_impl(int rank, void* buffer, size_t size, mr_t mr,
                         net_imm_data_t imm_data, void* user_context,
                         bool high_priority) override;
  error_t post_puts_impl(int rank, void* buffer, size_t size, uint64_t offset,
                         rmr_t rmr, void* user_context,
                         bool high_priority) override;
  error_t post_put_impl(int rank, void* buffer, size_t size, mr_t mr,
                        uint64_t offset, rmr_t rmr, void* user_context,
                        bool high_priority) override;
  error_t post_putImms_impl(int rank, void* buffer, size_t size,
                            uint64_t offset, rmr_t rmr, net_imm_data_t imm_data,
                            void* user_context, bool high_priority) override;
  error_t post_putImm_impl(int rank, void* buffer, size_t size, mr_t mr,
                           uint64_t offset, rmr_t rmr, net_imm_data_t imm_data,
                           void* user_context, bool high_priority) override;
  error_t post_get_impl(int rank, void* buffer, size_t size, mr_t mr,
                        uint64_t offset, rmr_t rmr, void* user_context,
                        bool high_priority) override;

  ibv_device_impl_t* p_ibv_device;
  std::vector<struct ibv_qp*> ib_qps;
  std::vector<LCISI_ibv_qp_extra_t> ib_qp_extras;
  std::vector<padded_atomic_t<int>> qp_remaining_slots;
  spinlock_t qps_lock;

 private:
  bool try_lock_qp(int rank);
  void unlock_qp(int rank);
  bool try_acquire_slot(int rank, bool high_priority);
  void release_slot(int rank);
};

}  // namespace lci

#endif  // LCI_BACKEND_IBV_BACKEND_IBV_HPP
