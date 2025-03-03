#ifndef LCI_BACKEND_IBV_INLINE_HPP
#define LCI_BACKEND_IBV_INLINE_HPP

namespace lci
{
inline rkey_t ibv_net_device_impl_t::get_rkey(mr_t mr)
{
  ibv_mr_impl_t& p_mr = *static_cast<ibv_mr_impl_t*>(mr.p_impl);
  return p_mr.ibv_mr->rkey;
}

inline std::vector<net_status_t> ibv_net_device_impl_t::poll_comp_impl(
    int max_polls)
{
  // TODO: Investigate the overhead of using a vector here.
  std::vector<net_status_t> statuses;
  LCI_Assert(max_polls > 0, "max_polls must be greater than 0\n");

  std::vector<struct ibv_wc> wcs(max_polls);

  if (!cq_lock.try_lock()) return statuses;
  int ne = ibv_poll_cq(ib_cq, max_polls, wcs.data());
  cq_lock.unlock();
  if (ne > 0) {
    // Got an entry here
    for (int i = 0; i < ne; i++) {
      LCI_Assert(wcs[i].status == IBV_WC_SUCCESS,
                 "Failed status %s (%d) for wr_id %d\n",
                 ibv_wc_status_str(wcs[i].status), wcs[i].status,
                 (int)wcs[i].wr_id);
      net_status_t status;
      if (wcs[i].opcode == IBV_WC_RECV) {
        status.opcode = net_opcode_t::RECV;
        status.user_context = (void*)wcs[i].wr_id;
        status.length = wcs[i].byte_len;
        status.imm_data = wcs[i].imm_data;
        status.rank = qp2rank[wcs[i].qp_num % qp2rank_mod];
      } else if (wcs[i].opcode == IBV_WC_RECV_RDMA_WITH_IMM) {
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
      statuses.push_back(status);
    }
  }
  return statuses;
}

namespace ibv_detail
{
inline uint32_t get_mr_lkey(mr_t mr)
{
  ibv_mr_impl_t& p_mr = *static_cast<ibv_mr_impl_t*>(mr.p_impl);
  return p_mr.ibv_mr->lkey;
}
}  // namespace ibv_detail

inline error_t ibv_net_device_impl_t::post_recv_impl(void* buffer, size_t size,
                                                     mr_t mr, void* ctx)
{
  struct ibv_sge list;
  list.addr = (uint64_t)buffer;
  list.length = size;
  list.lkey = ibv_detail::get_mr_lkey(mr);
  struct ibv_recv_wr wr;
  wr.wr_id = (uint64_t)ctx;
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

inline bool ibv_net_endpoint_impl_t::try_lock_qp(int rank)
{
  bool ret;
  if (!ib_qp_extras.empty()) {
    ret = ib_qp_extras[rank].lock.try_lock();
  } else {
    ret = true;
  }
  return ret;
}

inline void ibv_net_endpoint_impl_t::unlock_qp(int rank)
{
  if (!ib_qp_extras.empty()) {
    ib_qp_extras[rank].lock.unlock();
  }
}

inline error_t ibv_net_endpoint_impl_t::post_sends_impl(int rank, void* buffer,
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

inline error_t ibv_net_endpoint_impl_t::post_send_impl(int rank, void* buffer,
                                                       size_t size, mr_t mr,
                                                       net_imm_data_t imm_data,
                                                       void* ctx)
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
  wr.wr_id = (uintptr_t)ctx;
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
    return errorcode_t::ok;
  else if (ret == ENOMEM)
    return errorcode_t::retry_nomem;  // exceed send queue capacity
  else {
    IBV_SAFECALL_RET(ret);
  }
}

inline error_t ibv_net_endpoint_impl_t::post_puts_impl(int rank, void* buffer,
                                                       size_t size,
                                                       uintptr_t base,
                                                       uint64_t offset,
                                                       rkey_t rkey)
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

inline error_t ibv_net_endpoint_impl_t::post_put_impl(int rank, void* buffer,
                                                      size_t size, mr_t mr,
                                                      uintptr_t base,
                                                      uint64_t offset,
                                                      rkey_t rkey, void* ctx)
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
  wr.wr_id = (uint64_t)ctx;
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
    return errorcode_t::ok;
  else if (ret == ENOMEM)
    return errorcode_t::retry_nomem;  // exceed send queue capacity
  else {
    IBV_SAFECALL_RET(ret);
  }
}

inline error_t ibv_net_endpoint_impl_t::post_putImms_impl(
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

inline error_t ibv_net_endpoint_impl_t::post_putImm_impl(
    int rank, void* buffer, size_t size, mr_t mr, uintptr_t base,
    uint64_t offset, rkey_t rkey, net_imm_data_t imm_data, void* ctx)
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
  wr.wr_id = (uint64_t)ctx;
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
    return errorcode_t::ok;
  else if (ret == ENOMEM)
    return errorcode_t::retry_nomem;  // exceed send queue capacity
  else {
    IBV_SAFECALL_RET(ret);
  }
}

inline error_t ibv_net_endpoint_impl_t::post_get_impl(int rank, void* buffer,
                                                      size_t size, mr_t mr,
                                                      uintptr_t base,
                                                      uint64_t offset,
                                                      rkey_t rkey, void* ctx)
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
  wr.wr_id = (uint64_t)ctx;
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
    return errorcode_t::ok;
  else if (ret == ENOMEM)
    return errorcode_t::retry_nomem;  // exceed send queue capacity
  else {
    IBV_SAFECALL_RET(ret);
  }
}

}  // namespace lci

#endif  // LCI_BACKEND_IBV_INLINE_HPP