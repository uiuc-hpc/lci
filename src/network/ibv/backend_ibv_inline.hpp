// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#ifndef LCI_BACKEND_IBV_INLINE_HPP
#define LCI_BACKEND_IBV_INLINE_HPP

namespace lci
{
inline void qp2rank_map_t::add_qps(const std::vector<struct ibv_qp*>& qps)
{
  for (int i = 0; i < static_cast<int>(qps.size()); i++) {
    qp_rank_pairs.push_back(std::make_pair(qps[i]->qp_num, i));
  }
  calculate_map();
}

inline int qp2rank_map_t::get_rank(uint32_t qp_num)
{
  return qp2rank[qp_num % qp2rank_mod];
}

inline void qp2rank_map_t::calculate_map()
{
  auto start = std::chrono::high_resolution_clock::now();
  if (qp2rank.empty()) {
    qp2rank_mod = get_nranks();
    qp2rank.resize(qp2rank_mod, -1);
  }
  while (qp2rank_mod < INT32_MAX) {
    bool failed = false;
    for (auto qp_rank_pair : qp_rank_pairs) {
      int k = (qp_rank_pair.first % qp2rank_mod);
      if (qp2rank[k] != -1) {
        failed = true;
        break;
      }
      qp2rank[k] = qp_rank_pair.second;
    }
    if (!failed) break;
    qp2rank_mod++;
    qp2rank.resize(qp2rank_mod);
    std::fill(qp2rank.begin(), qp2rank.end(), -1);
  }
  LCI_Assert(qp2rank_mod != INT32_MAX,
             "Cannot find a suitable mod to hold qp2rank map\n");
  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  LCI_Log(LOG_INFO, "ibv", "qp2rank_mod is %d (for %lu qps), took %ld ms\n",
          qp2rank_mod, qp_rank_pairs.size(), duration.count());
}

inline rkey_t ibv_device_impl_t::get_rkey(mr_impl_t* mr)
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
    for (int i = 0; i < ne; i++) {
      LCI_Assert(wcs[i].status == IBV_WC_SUCCESS,
                 "Failed status %s (%d) for wr_id %p\n",
                 ibv_wc_status_str(wcs[i].status), wcs[i].status,
                 (void*)wcs[i].wr_id);
      if (!p_statuses) continue;
      net_status_t& status = p_statuses[i];
      if (wcs[i].opcode == IBV_WC_RECV) {
        status.opcode = net_opcode_t::RECV;
        status.user_context = (void*)wcs[i].wr_id;
        status.length = wcs[i].byte_len;
        status.imm_data = wcs[i].imm_data;
        status.rank = qp2rank_map.get_rank(wcs[i].qp_num);
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
    return errorcode_t::ok;
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

inline bool ibv_endpoint_impl_t::try_lock_qp(int rank)
{
  bool ret;
  if (!ib_qp_extras->empty()) {
    ret = (*ib_qp_extras)[rank].lock.try_lock();
  } else {
    ret = qps_lock->try_lock();
  }
  return ret;
}

inline void ibv_endpoint_impl_t::unlock_qp(int rank)
{
  if (!ib_qp_extras->empty()) {
    (*ib_qp_extras)[rank].lock.unlock();
  } else {
    qps_lock->unlock();
  }
}

inline error_t ibv_endpoint_impl_t::post_sends_impl(int rank, void* buffer,
                                                    size_t size,
                                                    net_imm_data_t imm_data)
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
  wr.wr_id = 0;
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

  struct ibv_send_wr* bad_wr;
  if (!try_lock_qp(rank)) return errorcode_t::retry_lock;
  int ret = ibv_post_send(ib_qps[rank], &wr, &bad_wr);
  unlock_qp(rank);
  if (ret == 0)
    return errorcode_t::ok;
  else if (ret == ENOMEM)
    return errorcode_t::retry_nomem;  // exceed send queue capacity
  else {
    IBV_SAFECALL_RET(ret);
  }
}

inline error_t ibv_endpoint_impl_t::post_send_impl(int rank, void* buffer,
                                                   size_t size, mr_t mr,
                                                   net_imm_data_t imm_data,
                                                   void* user_context)
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
  if (size <= net_context_attr.max_inject_size) {
    wr.send_flags |= IBV_SEND_INLINE;
  }

  struct ibv_send_wr* bad_wr;
  if (!try_lock_qp(rank)) return errorcode_t::retry_lock;
  int ret = ibv_post_send(ib_qps[rank], &wr, &bad_wr);
  unlock_qp(rank);
  if (ret == 0)
    return errorcode_t::posted;
  else if (ret == ENOMEM)
    return errorcode_t::retry_nomem;  // exceed send queue capacity
  else {
    IBV_SAFECALL_RET(ret);
  }
}

inline error_t ibv_endpoint_impl_t::post_puts_impl(int rank, void* buffer,
                                                   size_t size, uintptr_t base,
                                                   uint64_t offset, rkey_t rkey)
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
  wr.wr_id = 0;
  wr.next = NULL;
  wr.opcode = IBV_WR_RDMA_WRITE;
  wr.send_flags = IBV_SEND_SIGNALED | IBV_SEND_INLINE;
  wr.wr.rdma.remote_addr = (uintptr_t)(base + offset);
  wr.wr.rdma.rkey = rkey;

  struct ibv_send_wr* bad_wr;
  if (!try_lock_qp(rank)) return errorcode_t::retry_lock;
  int ret = ibv_post_send(ib_qps[rank], &wr, &bad_wr);
  unlock_qp(rank);
  if (ret == 0)
    return errorcode_t::ok;
  else if (ret == ENOMEM)
    return errorcode_t::retry_nomem;  // exceed send queue capacity
  else {
    IBV_SAFECALL_RET(ret);
  }
}

inline error_t ibv_endpoint_impl_t::post_put_impl(int rank, void* buffer,
                                                  size_t size, mr_t mr,
                                                  uintptr_t base,
                                                  uint64_t offset, rkey_t rkey,
                                                  void* user_context)
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
  wr.wr.rdma.remote_addr = (uintptr_t)(base + offset);
  wr.wr.rdma.rkey = rkey;
  if (size <= net_context_attr.max_inject_size) {
    wr.send_flags |= IBV_SEND_INLINE;
  }

  struct ibv_send_wr* bad_wr;
  if (!try_lock_qp(rank)) return errorcode_t::retry_lock;
  int ret = ibv_post_send(ib_qps[rank], &wr, &bad_wr);
  unlock_qp(rank);
  if (ret == 0)
    return errorcode_t::posted;
  else if (ret == ENOMEM)
    return errorcode_t::retry_nomem;  // exceed send queue capacity
  else {
    IBV_SAFECALL_RET(ret);
  }
}

inline error_t ibv_endpoint_impl_t::post_putImms_impl(
    int rank, void* buffer, size_t size, uintptr_t base, uint64_t offset,
    rkey_t rkey, net_imm_data_t imm_data)
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
  wr.wr_id = 0;
  wr.next = NULL;
  wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
  wr.send_flags = IBV_SEND_SIGNALED | IBV_SEND_INLINE;
  wr.wr.rdma.remote_addr = (uintptr_t)(base + offset);
  wr.wr.rdma.rkey = rkey;
  wr.imm_data = imm_data;

  struct ibv_send_wr* bad_wr;
  if (!try_lock_qp(rank)) return errorcode_t::retry_lock;
  int ret = ibv_post_send(ib_qps[rank], &wr, &bad_wr);
  unlock_qp(rank);
  if (ret == 0)
    return errorcode_t::ok;
  else if (ret == ENOMEM)
    return errorcode_t::retry_nomem;  // exceed send queue capacity
  else {
    IBV_SAFECALL_RET(ret);
  }
}

inline error_t ibv_endpoint_impl_t::post_putImm_impl(
    int rank, void* buffer, size_t size, mr_t mr, uintptr_t base,
    uint64_t offset, rkey_t rkey, net_imm_data_t imm_data, void* user_context)
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
  wr.wr.rdma.remote_addr = (uintptr_t)(base + offset);
  wr.wr.rdma.rkey = rkey;
  if (size <= net_context_attr.max_inject_size) {
    wr.send_flags |= IBV_SEND_INLINE;
  }

  struct ibv_send_wr* bad_wr;
  if (!try_lock_qp(rank)) return errorcode_t::retry_lock;
  int ret = ibv_post_send(ib_qps[rank], &wr, &bad_wr);
  unlock_qp(rank);
  if (ret == 0)
    return errorcode_t::posted;
  else if (ret == ENOMEM)
    return errorcode_t::retry_nomem;  // exceed send queue capacity
  else {
    IBV_SAFECALL_RET(ret);
  }
}

inline error_t ibv_endpoint_impl_t::post_get_impl(int rank, void* buffer,
                                                  size_t size, mr_t mr,
                                                  uintptr_t base,
                                                  uint64_t offset, rkey_t rkey,
                                                  void* user_context)
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
  wr.wr.rdma.remote_addr = (uintptr_t)(base + offset);
  wr.wr.rdma.rkey = rkey;
  if (size <= net_context_attr.max_inject_size) {
    wr.send_flags |= IBV_SEND_INLINE;
  }

  struct ibv_send_wr* bad_wr;
  if (!try_lock_qp(rank)) return errorcode_t::retry_lock;
  int ret = ibv_post_send(ib_qps[rank], &wr, &bad_wr);
  unlock_qp(rank);
  if (ret == 0)
    return errorcode_t::posted;
  else if (ret == ENOMEM)
    return errorcode_t::retry_nomem;  // exceed send queue capacity
  else {
    IBV_SAFECALL_RET(ret);
  }
}

}  // namespace lci

#endif  // LCI_BACKEND_IBV_INLINE_HPP