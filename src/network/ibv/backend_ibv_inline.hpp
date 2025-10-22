// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#ifndef LCI_BACKEND_IBV_INLINE_HPP
#define LCI_BACKEND_IBV_INLINE_HPP

namespace lci
{
inline void qp2rank_map_t::add_qps(const std::vector<struct ibv_qp*>& qps,
                                   std::vector<padded_atomic_t<int>>* qp_slots)
{
  if (qps.empty() || qp_slots == nullptr) return;
  std::unique_lock<std::shared_mutex> lock(mutex);
  for (size_t i = 0; i < qps.size(); ++i) {
    entry_t entry;
    entry.rank = static_cast<int>(i);
    entry.slots = &(*qp_slots)[i];
    qp_entries.emplace_back(qps[i]->qp_num, entry);
  }
  rebuild_locked();
}

inline void qp2rank_map_t::remove_qps(const std::vector<struct ibv_qp*>& qps)
{
  if (qps.empty()) return;
  std::unique_lock<std::shared_mutex> lock(mutex);
  if (qp_entries.empty()) {
    return;
  }
  for (auto* qp : qps) {
    auto it = std::remove_if(
        qp_entries.begin(), qp_entries.end(),
        [qp_num = qp->qp_num](const std::pair<uint32_t, entry_t>& pair) {
          return pair.first == qp_num;
        });
    if (it != qp_entries.end()) {
      qp_entries.erase(it, qp_entries.end());
    }
  }
  rebuild_locked();
}

inline qp2rank_map_t::entry_t qp2rank_map_t::snapshot_t::get_entry(
    uint32_t qp_num)
{
  if (table.empty()) {
    return entry_t{};
  }
  int idx = static_cast<int>(qp_num % mod);
  if (idx < 0 || idx >= static_cast<int>(table.size())) {
    return entry_t{};
  }
  return table[idx];
}

inline void qp2rank_map_t::rebuild_locked()
{
  auto start = std::chrono::high_resolution_clock::now();
  auto old_snapshot = current_snapshot.load(std::memory_order_relaxed);
  if (old_snapshot) {
    delete old_snapshot;
  }
  current_snapshot.store(nullptr, std::memory_order_relaxed);
  auto new_snapshot = new snapshot_t();
  if (qp_entries.empty()) {
    new_snapshot->mod = 1;
    new_snapshot->table.resize(1);
    current_snapshot.store(new_snapshot, std::memory_order_relaxed);
    return;
  }
  int mod = std::max(get_rank_n(), 1);
  std::vector<entry_t> table(mod);
  while (mod < INT32_MAX) {
    std::fill(table.begin(), table.end(), entry_t{});
    bool failed = false;
    for (auto& qp_entry : qp_entries) {
      int k = static_cast<int>(qp_entry.first % mod);
      if (table[k].slots != nullptr) {
        failed = true;
        break;
      }
      table[k] = qp_entry.second;
    }
    if (!failed) break;
    ++mod;
    table.resize(mod);
  }
  LCI_Assert(mod != INT32_MAX,
             "Cannot find a suitable mod to hold qp2rank map\n");
  new_snapshot->mod = mod;
  new_snapshot->table = std::move(table);
  current_snapshot.store(new_snapshot, std::memory_order_relaxed);
  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  LCI_Log(LOG_INFO, "ibv", "qp2rank_mod is %d (for %lu qps), took %ld ms\n",
          new_snapshot->mod, qp_entries.size(), duration.count());
}

inline uint64_t ibv_device_impl_t::get_rkey(mr_impl_t* mr)
{
  ibv_mr_impl_t& p_mr = *static_cast<ibv_mr_impl_t*>(mr);
  return p_mr.ibv_mr->rkey;
}

inline size_t ibv_device_impl_t::poll_comp_impl(net_status_t* p_statuses,
                                                size_t max_polls)
{
  struct ibv_wc wcs[LCI_BACKEND_MAX_POLLS];

  if (!cq_lock.try_lock()) return 0;
  int ne = ibv_poll_cq(ib_cq, max_polls, wcs);
  cq_lock.unlock();
  if (ne > 0) {
    // Got an entry here
    auto snapshot = qp2rank_map.get_snapshot();
    for (int i = 0; i < ne; i++) {
      LCI_Assert(wcs[i].status == IBV_WC_SUCCESS,
                 "Failed status %s (%d) for wr_id %p\n",
                 ibv_wc_status_str(wcs[i].status), wcs[i].status,
                 (void*)wcs[i].wr_id);
      qp2rank_map_t::entry_t entry = snapshot->get_entry(wcs[i].qp_num);
      // Increment SQ slots on any send-side completion
      if (wcs[i].opcode != IBV_WC_RECV &&
          wcs[i].opcode != IBV_WC_RECV_RDMA_WITH_IMM) {
        if (entry.rank >= 0 && entry.rank < get_rank_n() &&
            entry.slots != nullptr) {
          [[maybe_unused]] int prev =
              entry.slots->val.fetch_add(1, std::memory_order_relaxed);
          LCI_DBG_Assert(prev < static_cast<int>(attr.net_max_sends),
                         "Too many slots on QP for rank %d (prev %d)\n",
                         entry.rank, prev);
        } else {
          LCI_DBG_Log(
              LOG_WARN, "ibv",
              "Completion for qp_num %u does not map to an active endpoint\n",
              wcs[i].qp_num);
        }
      }
      if (!p_statuses) continue;
      net_status_t& status = p_statuses[i];
      memset(&status, 0, sizeof(status));
      if (wcs[i].opcode == IBV_WC_RECV) {
        status.opcode = net_opcode_t::RECV;
        status.user_context = (void*)wcs[i].wr_id;
        status.length = wcs[i].byte_len;
        status.imm_data = wcs[i].imm_data;
        status.rank = entry.rank;
      } else if (wcs[i].opcode == IBV_WC_RECV_RDMA_WITH_IMM) {
        consume_recvs(1);
        status.opcode = net_opcode_t::REMOTE_WRITE;
        status.user_context = (void*)wcs[i].wr_id;
        status.imm_data = wcs[i].imm_data;
      } else if (wcs[i].opcode == IBV_WC_SEND) {
        status.opcode = net_opcode_t::SEND;
        status.user_context = (void*)wcs[i].wr_id;
      } else if (wcs[i].opcode == IBV_WC_RDMA_WRITE) {
        status.opcode = net_opcode_t::WRITE;
        status.user_context = (void*)wcs[i].wr_id;
      } else {
        LCI_Assert(wcs[i].opcode == IBV_WC_RDMA_READ,
                   "Unexpected IBV opcode!\n");
        status.opcode = net_opcode_t::READ;
        status.user_context = (void*)wcs[i].wr_id;
      }
    }
  }
  return ne;
}

namespace ibv_detail
{
inline uint32_t get_mr_lkey(mr_t mr)
{
  ibv_mr_impl_t& p_mr = *static_cast<ibv_mr_impl_t*>(mr.p_impl);
  return p_mr.ibv_mr->lkey;
}
}  // namespace ibv_detail

inline error_t ibv_device_impl_t::post_recv_impl(void* buffer, size_t size,
                                                 mr_t mr, void* user_context)
{
  struct ibv_sge list;
  list.addr = (uint64_t)buffer;
  list.length = size;
  list.lkey = ibv_detail::get_mr_lkey(mr);
  struct ibv_recv_wr wr;
  wr.wr_id = (uint64_t)user_context;
  wr.next = NULL;
  wr.sg_list = &list;
  wr.num_sge = 1;
  struct ibv_recv_wr* bad_wr;

  if (!srq_lock.try_lock()) return errorcode_t::retry_lock;
  int ret = ibv_post_srq_recv(ib_srq, &wr, &bad_wr);
  srq_lock.unlock();

  if (ret == 0)
    return errorcode_t::done;
  else if (ret == ENOMEM)
    return errorcode_t::retry_nomem;  // exceed send queue capacity
  else {
    IBV_SAFECALL_RET(ret);
  }
}

inline size_t ibv_device_impl_t::post_recvs_impl(void* buffers[], size_t size,
                                                 size_t count, mr_t mr,
                                                 void* user_contexts[])
{
  struct ibv_sge list;
  list.length = size;
  list.lkey = ibv_detail::get_mr_lkey(mr);
  struct ibv_recv_wr wr;
  wr.next = NULL;
  wr.sg_list = &list;
  wr.num_sge = 1;
  struct ibv_recv_wr* bad_wr;

  int error;
  size_t n_posted = 0;

  if (!srq_lock.try_lock()) return 0;
  for (size_t i = 0; i < count; i++) {
    list.addr = (uint64_t)buffers[i];
    wr.wr_id = (uint64_t)user_contexts[i];
    error = ibv_post_srq_recv(ib_srq, &wr, &bad_wr);
    if (error == 0) {
      ++n_posted;
    } else {
      break;
    }
  }
  srq_lock.unlock();

  if (error == 0 || error == ENOMEM) {
    return n_posted;
  } else {
    IBV_SAFECALL(error);
    return 0;  // unreachable
  }
}

inline bool ibv_endpoint_impl_t::try_acquire_slot(int rank, bool high_priority)
{
  auto& counter = qp_remaining_slots[rank].val;
  double reserved_frac = device_attr.net_send_reserved_pct;
  int threshold = static_cast<int>(
      static_cast<double>(device_attr.net_max_sends) * reserved_frac);
  if (!high_priority && counter.load(std::memory_order_relaxed) <= threshold) {
    return false;
  }
  int prev = counter.fetch_sub(1, std::memory_order_relaxed);
  if (prev <= 0) {
    counter.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  return true;
}

// inline void ibv_endpoint_impl_t::acquire_slot(int rank)
// {
//   [[maybe_unused]] int prev = (*qp_remaining_slots)[rank].val.fetch_sub(1,
//   std::memory_order_relaxed); LCI_DBG_Assert(prev > 0, "Too many slots on QP
//   for rank %d (prev %d)\n", rank, prev);
// }

inline void ibv_endpoint_impl_t::release_slot(int rank)
{
  qp_remaining_slots[rank].val.fetch_add(1, std::memory_order_relaxed);
}

inline bool ibv_endpoint_impl_t::try_lock_qp(int rank)
{
  bool ret;
  if (!ib_qp_extras.empty()) {
    ret = ib_qp_extras[rank].lock.try_lock();
  } else {
    ret = qps_lock.try_lock();
  }
  return ret;
}

inline void ibv_endpoint_impl_t::unlock_qp(int rank)
{
  if (!ib_qp_extras.empty()) {
    ib_qp_extras[rank].lock.unlock();
  } else {
    qps_lock.unlock();
  }
}

inline error_t ibv_endpoint_impl_t::post_sends_impl(int rank, void* buffer,
                                                    size_t size,
                                                    net_imm_data_t imm_data,
                                                    void* user_context,
                                                    bool high_priority)
{
  LCI_Assert(size <= net_context_attr.max_inject_size,
             "%lu exceed the inline message size\n"
             "limit! %lu\n",
             size, net_context_attr.max_inject_size);
  struct ibv_sge list;
  struct ibv_send_wr wr;
  if (LCT_likely(size > 0)) {
    list.addr = (uint64_t)buffer;
    list.length = size;
    list.lkey = 0;
    wr.sg_list = &list;
    wr.num_sge = 1;
  } else {
    // With mlx4 backend, sge.length = 0 will be treated as 0x80000000.
    // With mlx5 backend, it will just be 0.
    // So we better just set num_sge to 0 here.
    wr.sg_list = NULL;
    wr.num_sge = 0;
  }
  wr.wr_id = (uintptr_t)user_context;
  wr.next = NULL;
  wr.opcode = IBV_WR_SEND_WITH_IMM;
  wr.send_flags = IBV_SEND_INLINE;
  wr.imm_data = imm_data;

  //  static int ninline = 0;
  //  int ninline_old = __sync_fetch_and_add(&ninline, 1);
  //  if (ninline_old == 63) {
  wr.send_flags |= IBV_SEND_SIGNALED;
  //    ninline = 0;
  //  }

  if (!try_acquire_slot(rank, high_priority)) {
    return errorcode_t::retry_nomem;
  }
  struct ibv_send_wr* bad_wr;
  if (!try_lock_qp(rank)) {
    release_slot(rank);
    return errorcode_t::retry_lock;
  }
  int ret = ibv_post_send(ib_qps[rank], &wr, &bad_wr);
  unlock_qp(rank);
  if (ret == 0) {
    return errorcode_t::done;
  } else if (ret == ENOMEM) {
    release_slot(rank);
    return errorcode_t::retry_nomem;  // exceed send queue capacity
  } else {
    release_slot(rank);
    IBV_SAFECALL_RET(ret);
  }
}

inline error_t ibv_endpoint_impl_t::post_send_impl(int rank, void* buffer,
                                                   size_t size, mr_t mr,
                                                   net_imm_data_t imm_data,
                                                   void* user_context,
                                                   bool high_priority)
{
  struct ibv_sge list;
  struct ibv_send_wr wr;
  if (LCT_likely(size > 0)) {
    list.addr = (uint64_t)buffer;
    list.length = size;
    list.lkey = ibv_detail::get_mr_lkey(mr);
    wr.sg_list = &list;
    wr.num_sge = 1;
  } else {
    // With mlx4 backend, sge.length = 0 will be treated as 0x80000000.
    // With mlx5 backend, it will just be 0.
    // So we better just set num_sge to 0 here.
    wr.sg_list = NULL;
    wr.num_sge = 0;
  }
  wr.wr_id = (uintptr_t)user_context;
  wr.next = NULL;
  wr.opcode = IBV_WR_SEND_WITH_IMM;
  wr.send_flags = IBV_SEND_SIGNALED;
  wr.imm_data = imm_data;

  if (!try_acquire_slot(rank, high_priority)) {
    return errorcode_t::retry_nomem;
  }
  struct ibv_send_wr* bad_wr;
  if (!try_lock_qp(rank)) {
    release_slot(rank);
    return errorcode_t::retry_lock;
  }
  int ret = ibv_post_send(ib_qps[rank], &wr, &bad_wr);
  unlock_qp(rank);
  if (ret == 0)
    return errorcode_t::posted;
  else if (ret == ENOMEM) {
    release_slot(rank);
    return errorcode_t::retry_nomem;  // exceed send queue capacity
  } else {
    release_slot(rank);
    IBV_SAFECALL_RET(ret);
  }
}

inline error_t ibv_endpoint_impl_t::post_puts_impl(int rank, void* buffer,
                                                   size_t size, uint64_t offset,
                                                   rmr_t rmr,
                                                   void* user_context,
                                                   bool high_priority)
{
  LCI_Assert(size <= net_context_attr.max_inject_size,
             "%lu exceed the inline message size\n"
             "limit! %lu\n",
             size, net_context_attr.max_inject_size);

  struct ibv_sge list;
  struct ibv_send_wr wr;
  if (LCT_likely(size > 0)) {
    list.addr = (uint64_t)buffer;
    list.length = size;
    list.lkey = 0;
    wr.sg_list = &list;
    wr.num_sge = 1;
  } else {
    // With mlx4 backend, sge.length = 0 will be treated as 0x80000000.
    // With mlx5 backend, it will just be 0.
    // So we better just set num_sge to 0 here.
    wr.sg_list = NULL;
    wr.num_sge = 0;
  }
  wr.wr_id = (uintptr_t)user_context;
  wr.next = NULL;
  wr.opcode = IBV_WR_RDMA_WRITE;
  wr.send_flags = IBV_SEND_SIGNALED | IBV_SEND_INLINE;
  wr.wr.rdma.remote_addr = (uintptr_t)(rmr.base + offset);
  wr.wr.rdma.rkey = rmr.opaque_rkey;

  if (!try_acquire_slot(rank, high_priority)) {
    return errorcode_t::retry_nomem;
  }
  struct ibv_send_wr* bad_wr;
  if (!try_lock_qp(rank)) {
    release_slot(rank);
    return errorcode_t::retry_lock;
  }
  int ret = ibv_post_send(ib_qps[rank], &wr, &bad_wr);
  unlock_qp(rank);
  if (ret == 0)
    return errorcode_t::done;
  else if (ret == ENOMEM) {
    release_slot(rank);
    return errorcode_t::retry_nomem;  // exceed send queue capacity
  } else {
    release_slot(rank);
    IBV_SAFECALL_RET(ret);
  }
}

inline error_t ibv_endpoint_impl_t::post_put_impl(int rank, void* buffer,
                                                  size_t size, mr_t mr,

                                                  uint64_t offset, rmr_t rmr,
                                                  void* user_context,
                                                  bool high_priority)
{
  struct ibv_sge list;
  struct ibv_send_wr wr;
  if (LCT_likely(size > 0)) {
    list.addr = (uint64_t)buffer;
    list.length = size;
    list.lkey = ibv_detail::get_mr_lkey(mr);
    wr.sg_list = &list;
    wr.num_sge = 1;
  } else {
    // With mlx4 backend, sge.length = 0 will be treated as 0x80000000.
    // With mlx5 backend, it will just be 0.
    // So we better just set num_sge to 0 here.
    wr.sg_list = NULL;
    wr.num_sge = 0;
  }
  wr.wr_id = (uint64_t)user_context;
  wr.next = NULL;
  wr.opcode = IBV_WR_RDMA_WRITE;
  wr.send_flags = IBV_SEND_SIGNALED;
  wr.wr.rdma.remote_addr = (uintptr_t)(rmr.base + offset);
  wr.wr.rdma.rkey = rmr.opaque_rkey;

  if (!try_acquire_slot(rank, high_priority)) {
    return errorcode_t::retry_nomem;
  }
  struct ibv_send_wr* bad_wr;
  if (!try_lock_qp(rank)) {
    release_slot(rank);
    return errorcode_t::retry_lock;
  }
  int ret = ibv_post_send(ib_qps[rank], &wr, &bad_wr);
  unlock_qp(rank);
  if (ret == 0)
    return errorcode_t::posted;
  else if (ret == ENOMEM) {
    release_slot(rank);
    return errorcode_t::retry_nomem;  // exceed send queue capacity
  } else {
    release_slot(rank);
    IBV_SAFECALL_RET(ret);
  }
}

inline error_t ibv_endpoint_impl_t::post_putImms_impl(
    int rank, void* buffer, size_t size, uint64_t offset, rmr_t rmr,
    net_imm_data_t imm_data, void* user_context, bool high_priority)
{
  LCI_Assert(size <= net_context_attr.max_inject_size,
             "%lu exceed the inline message size\n"
             "limit! %lu\n",
             size, net_context_attr.max_inject_size);
  struct ibv_sge list;
  struct ibv_send_wr wr;
  if (LCT_likely(size > 0)) {
    list.addr = (uint64_t)buffer;
    list.length = size;
    list.lkey = 0;
    wr.sg_list = &list;
    wr.num_sge = 1;
  } else {
    // With mlx4 backend, sge.length = 0 will be treated as 0x80000000.
    // With mlx5 backend, it will just be 0.
    // So we better just set num_sge to 0 here.
    wr.sg_list = NULL;
    wr.num_sge = 0;
  }
  wr.wr_id = (uintptr_t)user_context;
  wr.next = NULL;
  wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
  wr.send_flags = IBV_SEND_SIGNALED | IBV_SEND_INLINE;
  wr.wr.rdma.remote_addr = (uintptr_t)(rmr.base + offset);
  wr.wr.rdma.rkey = rmr.opaque_rkey;
  wr.imm_data = imm_data;

  if (!try_acquire_slot(rank, high_priority)) {
    return errorcode_t::retry_nomem;
  }
  struct ibv_send_wr* bad_wr;
  if (!try_lock_qp(rank)) {
    release_slot(rank);
    return errorcode_t::retry_lock;
  }
  int ret = ibv_post_send(ib_qps[rank], &wr, &bad_wr);
  unlock_qp(rank);
  if (ret == 0)
    return errorcode_t::done;
  else if (ret == ENOMEM) {
    release_slot(rank);
    return errorcode_t::retry_nomem;  // exceed send queue capacity
  } else {
    release_slot(rank);
    IBV_SAFECALL_RET(ret);
  }
}

inline error_t ibv_endpoint_impl_t::post_putImm_impl(
    int rank, void* buffer, size_t size, mr_t mr, uint64_t offset, rmr_t rmr,
    net_imm_data_t imm_data, void* user_context, bool high_priority)
{
  struct ibv_sge list;
  struct ibv_send_wr wr;
  if (LCT_likely(size > 0)) {
    list.addr = (uint64_t)buffer;
    list.length = size;
    list.lkey = ibv_detail::get_mr_lkey(mr);
    wr.sg_list = &list;
    wr.num_sge = 1;
  } else {
    // With mlx4 backend, sge.length = 0 will be treated as 0x80000000.
    // With mlx5 backend, it will just be 0.
    // So we better just set num_sge to 0 here.
    wr.sg_list = NULL;
    wr.num_sge = 0;
  }
  wr.wr_id = (uint64_t)user_context;
  wr.next = NULL;
  wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
  wr.send_flags = IBV_SEND_SIGNALED;
  wr.imm_data = imm_data;
  wr.wr.rdma.remote_addr = (uintptr_t)(rmr.base + offset);
  wr.wr.rdma.rkey = rmr.opaque_rkey;

  if (!try_acquire_slot(rank, high_priority)) {
    return errorcode_t::retry_nomem;
  }
  struct ibv_send_wr* bad_wr;
  if (!try_lock_qp(rank)) {
    release_slot(rank);
    return errorcode_t::retry_lock;
  }
  int ret = ibv_post_send(ib_qps[rank], &wr, &bad_wr);
  unlock_qp(rank);
  if (ret == 0)
    return errorcode_t::posted;
  else if (ret == ENOMEM) {
    release_slot(rank);
    return errorcode_t::retry_nomem;  // exceed send queue capacity
  } else {
    release_slot(rank);
    IBV_SAFECALL_RET(ret);
  }
}

inline error_t ibv_endpoint_impl_t::post_get_impl(int rank, void* buffer,
                                                  size_t size, mr_t mr,

                                                  uint64_t offset, rmr_t rmr,
                                                  void* user_context,
                                                  bool high_priority)
{
  struct ibv_sge list;
  struct ibv_send_wr wr;
  if (LCT_likely(size > 0)) {
    list.addr = (uint64_t)buffer;
    list.length = size;
    list.lkey = ibv_detail::get_mr_lkey(mr);
    wr.sg_list = &list;
    wr.num_sge = 1;
  } else {
    // With mlx4 backend, sge.length = 0 will be treated as 0x80000000.
    // With mlx5 backend, it will just be 0.
    // So we better just set num_sge to 0 here.
    wr.sg_list = NULL;
    wr.num_sge = 0;
  }
  wr.wr_id = (uint64_t)user_context;
  wr.next = NULL;
  wr.opcode = IBV_WR_RDMA_READ;
  wr.send_flags = IBV_SEND_SIGNALED;
  wr.wr.rdma.remote_addr = (uintptr_t)(rmr.base + offset);
  wr.wr.rdma.rkey = rmr.opaque_rkey;

  if (!try_acquire_slot(rank, high_priority)) {
    return errorcode_t::retry_nomem;
  }
  struct ibv_send_wr* bad_wr;
  if (!try_lock_qp(rank)) {
    release_slot(rank);
    return errorcode_t::retry_lock;
  }
  int ret = ibv_post_send(ib_qps[rank], &wr, &bad_wr);
  unlock_qp(rank);
  if (ret == 0)
    return errorcode_t::posted;
  else if (ret == ENOMEM) {
    release_slot(rank);
    return errorcode_t::retry_nomem;  // exceed send queue capacity
  } else {
    release_slot(rank);
    IBV_SAFECALL_RET(ret);
  }
}

}  // namespace lci

#endif  // LCI_BACKEND_IBV_INLINE_HPP
