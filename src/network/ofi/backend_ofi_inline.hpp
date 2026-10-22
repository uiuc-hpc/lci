// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#ifndef LCI_BACKEND_OFI_BACKEND_OFI_INLINE_HPP
#define LCI_BACKEND_OFI_BACKEND_OFI_INLINE_HPP

namespace lci
{
inline uint64_t ofi_device_impl_t::get_rkey(mr_impl_t* mr)
{
  ofi_mr_impl_t& p_mr = *static_cast<ofi_mr_impl_t*>(mr);
  return fi_mr_key((struct fid_mr*)(p_mr.ofi_mr));
}

inline size_t ofi_device_impl_t::poll_comp_impl(net_status_t* p_statuses,
                                                size_t max_polls)
{
  struct fi_cq_data_entry fi_entries[LCI_BACKEND_MAX_POLLS];
  fi_addr_t src_addrs[LCI_BACKEND_MAX_POLLS];

  // Keep the configured polling lock across both reads so another poller
  // cannot consume the error entry reported by fi_cq_read.
  std::unique_lock<spinlock_t> poll_lock(lock, std::defer_lock);
  if ((ofi_lock_mode & LCI_NET_TRYLOCK_POLL) != 0 && !poll_lock.try_lock()) {
    return 0;
  }

  ssize_t ne = rank_from_src
                   ? fi_cq_readfrom(ofi_cq, fi_entries, max_polls, src_addrs)
                   : fi_cq_read(ofi_cq, fi_entries, max_polls);
  if (ne > 0) {
    // Got an entry here
    for (int j = 0; j < ne; j++) {
      if (p_statuses) {
        fill_status(p_statuses[j], fi_entries[j],
                    rank_from_src ? src_addrs[j] : FI_ADDR_NOTAVAIL);
      }
    }
  } else if (ne == -FI_EAGAIN) {
    ne = 0;
  } else {
    LCI_Assert(ne == -FI_EAVAIL, "unexpected return error: %s\n",
               fi_strerror(-ne));
    struct fi_cq_err_entry error = {};
    char err_data[64];
    error.err_data = err_data;
    error.err_data_size = sizeof(err_data);
    ssize_t ret_cqerr = fi_cq_readerr(ofi_cq, &error, 0);
    if (ret_cqerr == -FI_EAGAIN) {
      return 0;
    } else {
      LCI_Assert(ret_cqerr == 1, "fi_cq_readerr failed: %s\n",
                 fi_strerror(-ret_cqerr));
      if (p_statuses) {
        net_status_t& status = p_statuses[0];
        memset(&status, 0, sizeof(status));
        status.opcode = net_opcode_t::ERROR;
        status.rank = -1;
        bool is_outgoing =
            (error.flags & (FI_SEND | FI_WRITE | FI_READ)) != 0 &&
            (error.flags & (FI_RECV | FI_REMOTE_WRITE)) == 0;
        if (use_op_ctx && error.op_context != nullptr) {
          is_outgoing =
              static_cast<ofi_detail::op_ctx_t*>(error.op_context)->opcode !=
              net_opcode_t::RECV;
        }
        void* user_context = unwrap_context(error.op_context);
        status.user_context = is_outgoing ? user_context : nullptr;
      }
      ne = 1;
    }
  }
  return static_cast<size_t>(ne);
}

inline void ofi_device_impl_t::fill_status(net_status_t& status,
                                           const struct fi_cq_data_entry& entry,
                                           fi_addr_t src_addr)
{
  memset(&status, 0, sizeof(status));
  if (use_op_ctx) {
    // The wrapper says which operation completed. No context means the
    // target side of a write with immediate data; a receive that a remote
    // write consumed reports FI_REMOTE_WRITE.
    if (entry.op_context == nullptr) {
      status.opcode = net_opcode_t::REMOTE_WRITE;
    } else {
      auto* op_ctx = static_cast<ofi_detail::op_ctx_t*>(entry.op_context);
      status.opcode = op_ctx->opcode;
      if (status.opcode == net_opcode_t::RECV &&
          (entry.flags & FI_REMOTE_WRITE))
        status.opcode = net_opcode_t::REMOTE_WRITE;
      status.user_context = unwrap_context(entry.op_context);
    }
  } else if (entry.flags & FI_RECV) {
    status.opcode = net_opcode_t::RECV;
    status.user_context = entry.op_context;
  } else if (entry.flags & FI_REMOTE_WRITE) {
    status.opcode = net_opcode_t::REMOTE_WRITE;
  } else if (entry.flags & FI_SEND) {
    status.opcode = net_opcode_t::SEND;
    status.user_context = entry.op_context;
  } else if (entry.flags & FI_WRITE) {
    status.opcode = net_opcode_t::WRITE;
    status.user_context = entry.op_context;
  } else {
    LCI_DBG_Assert(entry.flags & FI_READ, "Unexpected OFI opcode!\n");
    status.opcode = net_opcode_t::READ;
    status.user_context = entry.op_context;
  }
  if (status.opcode == net_opcode_t::RECV) {
    status.length = entry.len;
    status.imm_data = entry.data & ((1ULL << 32) - 1);
    if (rank_from_src) {
      LCI_DBG_Assert(src_addr != FI_ADDR_NOTAVAIL,
                     "Receive without a source address\n");
      status.rank = (int)src_addr;
    } else {
      status.rank = (int)(entry.data >> 32);
    }
  } else if (status.opcode == net_opcode_t::REMOTE_WRITE) {
    status.imm_data = entry.data;
  }
}

namespace ofi_detail
{
inline void* get_mr_desc(mr_t mr)
{
  return fi_mr_desc(static_cast<ofi_mr_impl_t*>(mr.p_impl)->ofi_mr);
}

inline uintptr_t get_remote_addr(rmr_t rmr, uint64_t offset, uint64_t mr_mode)
{
  if (mr_mode & FI_MR_VIRT_ADDR) {
    return rmr.base + offset;
  } else {
    return (rmr.base - rmr.mr_base) + offset;
  }
}
}  // namespace ofi_detail

inline error_t ofi_device_impl_t::post_recv_impl(void* buffer, size_t size,
                                                 mr_t mr, void* user_context)
{
  auto mr_desc = fi_mr_desc(static_cast<ofi_mr_impl_t*>(mr.p_impl)->ofi_mr);
  LCI_OFI_CS_TRY_ENTER(LCI_NET_TRYLOCK_RECV, errorcode_t::retry_lock);
  void* ctx = wrap_context(user_context, net_opcode_t::RECV);
  ssize_t ret = fi_recv(ofi_ep, buffer, size, mr_desc, FI_ADDR_UNSPEC, ctx);
  if (ret != FI_SUCCESS) discard_context(ctx);
  LCI_OFI_CS_EXIT(LCI_NET_TRYLOCK_RECV);
  if (ret == FI_SUCCESS)
    return errorcode_t::done;
  else if (ret == -FI_EAGAIN)
    return errorcode_t::retry_nomem;
  else {
    FI_SAFECALL_RET(ret);
  }
}

inline size_t ofi_device_impl_t::post_recvs_impl(void* buffers[], size_t size,
                                                 size_t count, mr_t mr,
                                                 void* user_contexts[])
{
  auto mr_desc = fi_mr_desc(static_cast<ofi_mr_impl_t*>(mr.p_impl)->ofi_mr);
  LCI_OFI_CS_TRY_ENTER(LCI_NET_TRYLOCK_RECV, 0);

  ssize_t error;
  size_t n_posted = 0;
  for (size_t i = 0; i < count; i++) {
    void* ctx = wrap_context(user_contexts[i], net_opcode_t::RECV);
    error = fi_recv(ofi_ep, buffers[i], size, mr_desc, FI_ADDR_UNSPEC, ctx);
    if (error == FI_SUCCESS) {
      ++n_posted;
    } else {
      discard_context(ctx);
      break;
    }
  }
  LCI_OFI_CS_EXIT(LCI_NET_TRYLOCK_RECV);
  if (error == FI_SUCCESS || error == -FI_EAGAIN)
    return n_posted;
  else {
    FI_SAFECALL(error);
    return 0;  // unreachable
  }
}

inline error_t ofi_endpoint_impl_t::post_sends_impl(int rank, void* buffer,
                                                    size_t size,
                                                    net_imm_data_t imm_data,
                                                    void* user_context,
                                                    bool /*high_priority*/)
{
  struct iovec iov;
  iov.iov_base = buffer;
  iov.iov_len = size;
  struct fi_msg msg;
  msg.msg_iov = &iov;
  msg.desc = nullptr;
  msg.iov_count = 1;
  msg.addr = peer_addrs[rank];
  msg.data = rank_cq_bits | imm_data;
  LCI_OFI_CS_TRY_ENTER(LCI_NET_TRYLOCK_SEND, errorcode_t::retry_lock);
  void* ctx = p_ofi_device->wrap_context(user_context, net_opcode_t::SEND);
  msg.context = ctx;
  ssize_t ret =
      fi_sendmsg(ofi_ep, &msg, FI_INJECT | FI_COMPLETION | FI_REMOTE_CQ_DATA);

  // ssize_t ret =
  //     fi_injectdata(ofi_ep, buffer, size, (uint64_t)my_rank << 32 | imm_data,
  //                   peer_addrs[rank]);
  if (ret != FI_SUCCESS) p_ofi_device->discard_context(ctx);
  LCI_OFI_CS_EXIT(LCI_NET_TRYLOCK_SEND);
  if (ret == FI_SUCCESS)
    return errorcode_t::done;
  else if (ret == -FI_EAGAIN)
    return errorcode_t::retry_nomem;
  else {
    FI_SAFECALL_RET(ret);
  }
}

inline error_t ofi_endpoint_impl_t::post_send_impl(int rank, void* buffer,
                                                   size_t size, mr_t mr,
                                                   net_imm_data_t imm_data,
                                                   void* user_context,
                                                   bool /*high_priority*/)
{
  LCI_OFI_CS_TRY_ENTER(LCI_NET_TRYLOCK_SEND, errorcode_t::retry_lock);
  void* ctx = p_ofi_device->wrap_context(user_context, net_opcode_t::SEND);
  ssize_t ret = fi_senddata(ofi_ep, buffer, size, ofi_detail::get_mr_desc(mr),
                            rank_cq_bits | imm_data, peer_addrs[rank], ctx);
  if (ret != FI_SUCCESS) p_ofi_device->discard_context(ctx);
  LCI_OFI_CS_EXIT(LCI_NET_TRYLOCK_SEND);
  if (ret == FI_SUCCESS)
    return errorcode_t::posted;
  else if (ret == -FI_EAGAIN)
    return errorcode_t::retry_nomem;
  else {
    FI_SAFECALL_RET(ret);
  }
}

inline error_t ofi_endpoint_impl_t::post_puts_impl(int rank, void* buffer,
                                                   size_t size, uint64_t offset,
                                                   rmr_t rmr,
                                                   void* user_context,
                                                   bool /*high_priority*/)
{
  uintptr_t addr =
      ofi_detail::get_remote_addr(rmr, offset, ofi_domain_attr->mr_mode);
  struct fi_msg_rma msg;
  struct iovec iov;
  struct fi_rma_iov riov;
  iov.iov_base = buffer;
  iov.iov_len = size;
  msg.msg_iov = &iov;
  msg.desc = NULL;
  msg.iov_count = 1;
  msg.addr = peer_addrs[rank];
  riov.addr = addr;
  riov.len = size;
  riov.key = rmr.opaque_rkey;
  msg.rma_iov = &riov;
  msg.rma_iov_count = 1;
  msg.data = 0;
  LCI_OFI_CS_TRY_ENTER(LCI_NET_TRYLOCK_SEND, errorcode_t::retry_lock);
  void* ctx = p_ofi_device->wrap_context(user_context, net_opcode_t::WRITE);
  msg.context = ctx;
  ssize_t ret = fi_writemsg(ofi_ep, &msg,
                            FI_INJECT | FI_COMPLETION | FI_DELIVERY_COMPLETE);
  if (ret != FI_SUCCESS) p_ofi_device->discard_context(ctx);
  LCI_OFI_CS_EXIT(LCI_NET_TRYLOCK_SEND);
  if (ret == FI_SUCCESS)
    return errorcode_t::done;
  else if (ret == -FI_EAGAIN)
    return errorcode_t::retry_nomem;
  else {
    FI_SAFECALL_RET(ret);
  }
}

inline error_t ofi_endpoint_impl_t::post_put_impl(int rank, void* buffer,
                                                  size_t size, mr_t mr,
                                                  uint64_t offset, rmr_t rmr,
                                                  void* user_context,
                                                  bool /*high_priority*/)
{
  uintptr_t addr =
      ofi_detail::get_remote_addr(rmr, offset, ofi_domain_attr->mr_mode);
  struct fi_msg_rma msg;
  struct iovec iov;
  struct fi_rma_iov riov;
  void* desc = ofi_detail::get_mr_desc(mr);
  iov.iov_base = buffer;
  iov.iov_len = size;
  msg.msg_iov = &iov;
  msg.desc = &desc;
  msg.iov_count = 1;
  msg.addr = peer_addrs[rank];
  riov.addr = addr;
  riov.len = size;
  riov.key = rmr.opaque_rkey;
  msg.rma_iov = &riov;
  msg.rma_iov_count = 1;
  msg.data = 0;
  LCI_OFI_CS_TRY_ENTER(LCI_NET_TRYLOCK_SEND, errorcode_t::retry_lock);
  void* ctx = p_ofi_device->wrap_context(user_context, net_opcode_t::WRITE);
  msg.context = ctx;
  ssize_t ret = fi_writemsg(ofi_ep, &msg, FI_COMPLETION | FI_DELIVERY_COMPLETE);
  if (ret != FI_SUCCESS) p_ofi_device->discard_context(ctx);
  LCI_OFI_CS_EXIT(LCI_NET_TRYLOCK_SEND);
  if (ret == FI_SUCCESS)
    return errorcode_t::posted;
  else if (ret == -FI_EAGAIN)
    return errorcode_t::retry_nomem;
  else {
    FI_SAFECALL_RET(ret);
  }
}

inline error_t ofi_endpoint_impl_t::cxi_inject_writedata_workaround(
    int rank, void* buffer, size_t size, uint64_t offset, rmr_t rmr,
    net_imm_data_t imm_data, void* user_context)
{
  uintptr_t addr =
      ofi_detail::get_remote_addr(rmr, offset, ofi_domain_attr->mr_mode);
  LCI_OFI_CS_TRY_ENTER(LCI_NET_TRYLOCK_SEND, errorcode_t::retry_lock);
  ssize_t ret = fi_inject_writedata(ofi_ep, buffer, size, imm_data,
                                    peer_addrs[rank], addr, rmr.opaque_rkey);
  LCI_OFI_CS_EXIT(LCI_NET_TRYLOCK_SEND);
  if (ret == FI_SUCCESS) {
    // fi_inject_writedata does not generate a local CQ completion. FI_INJECT
    // has already copied the source buffer, so retire the internal context
    // here instead of leaving the endpoint's pending operation count stuck.
    if (user_context) {
      free_ctx_and_signal_comp(static_cast<internal_context_t*>(user_context));
    }
    return errorcode_t::done;
  } else if (ret == -FI_EAGAIN)
    return errorcode_t::retry_nomem;
  else {
    FI_SAFECALL_RET(ret);
  }
}

inline error_t ofi_endpoint_impl_t::post_putImms_impl(
    int rank, void* buffer, size_t size, uint64_t offset, rmr_t rmr,
    net_imm_data_t imm_data, void* user_context, bool /*high_priority*/)
{
  if (p_ofi_device->use_cxi_writedata) {
    return cxi_inject_writedata_workaround(rank, buffer, size, offset, rmr,
                                           imm_data, user_context);
  }
  uintptr_t addr =
      ofi_detail::get_remote_addr(rmr, offset, ofi_domain_attr->mr_mode);
  struct fi_msg_rma msg;
  struct iovec iov;
  struct fi_rma_iov riov;
  iov.iov_base = buffer;
  iov.iov_len = size;
  msg.msg_iov = &iov;
  msg.desc = nullptr;
  msg.iov_count = 1;
  msg.addr = peer_addrs[rank];
  riov.addr = addr;
  riov.len = size;
  riov.key = rmr.opaque_rkey;
  msg.rma_iov = &riov;
  msg.rma_iov_count = 1;
  msg.data = imm_data;
  LCI_OFI_CS_TRY_ENTER(LCI_NET_TRYLOCK_SEND, errorcode_t::retry_lock);
  void* ctx = p_ofi_device->wrap_context(user_context, net_opcode_t::WRITE);
  msg.context = ctx;
  ssize_t ret = fi_writemsg(
      ofi_ep, &msg,
      FI_INJECT | FI_COMPLETION | FI_DELIVERY_COMPLETE | FI_REMOTE_CQ_DATA);
  if (ret != FI_SUCCESS) p_ofi_device->discard_context(ctx);
  LCI_OFI_CS_EXIT(LCI_NET_TRYLOCK_SEND);
  if (ret == FI_SUCCESS)
    return errorcode_t::done;
  else if (ret == -FI_EAGAIN)
    return errorcode_t::retry_nomem;
  else {
    FI_SAFECALL_RET(ret);
  }
}

inline error_t ofi_endpoint_impl_t::cxi_writedata_workaround(
    int rank, void* buffer, size_t size, mr_t mr, uint64_t offset, rmr_t rmr,
    net_imm_data_t imm_data, void* user_context)
{
  uintptr_t addr =
      ofi_detail::get_remote_addr(rmr, offset, ofi_domain_attr->mr_mode);
  LCI_OFI_CS_TRY_ENTER(LCI_NET_TRYLOCK_SEND, errorcode_t::retry_lock);
  ssize_t ret =
      fi_writedata(ofi_ep, buffer, size, ofi_detail::get_mr_desc(mr), imm_data,
                   peer_addrs[rank], addr, rmr.opaque_rkey, user_context);
  LCI_OFI_CS_EXIT(LCI_NET_TRYLOCK_SEND);
  if (ret == FI_SUCCESS)
    return errorcode_t::posted;
  else if (ret == -FI_EAGAIN)
    return errorcode_t::retry_nomem;
  else {
    FI_SAFECALL_RET(ret);
  }
}

inline error_t ofi_endpoint_impl_t::post_putImm_impl(
    int rank, void* buffer, size_t size, mr_t mr, uint64_t offset, rmr_t rmr,
    net_imm_data_t imm_data, void* user_context, bool /*high_priority*/)
{
  if (p_ofi_device->use_cxi_writedata) {
    return cxi_writedata_workaround(rank, buffer, size, mr, offset, rmr,
                                    imm_data, user_context);
  }
  uintptr_t addr =
      ofi_detail::get_remote_addr(rmr, offset, ofi_domain_attr->mr_mode);
  struct fi_msg_rma msg;
  struct iovec iov;
  struct fi_rma_iov riov;
  void* desc = ofi_detail::get_mr_desc(mr);
  iov.iov_base = buffer;
  iov.iov_len = size;
  msg.msg_iov = &iov;
  msg.desc = &desc;
  msg.iov_count = 1;
  msg.addr = peer_addrs[rank];
  riov.addr = addr;
  riov.len = size;
  riov.key = rmr.opaque_rkey;
  msg.rma_iov = &riov;
  msg.rma_iov_count = 1;
  msg.data = imm_data;
  LCI_OFI_CS_TRY_ENTER(LCI_NET_TRYLOCK_SEND, errorcode_t::retry_lock);
  void* ctx = p_ofi_device->wrap_context(user_context, net_opcode_t::WRITE);
  msg.context = ctx;
  ssize_t ret = fi_writemsg(
      ofi_ep, &msg, FI_COMPLETION | FI_DELIVERY_COMPLETE | FI_REMOTE_CQ_DATA);
  if (ret != FI_SUCCESS) p_ofi_device->discard_context(ctx);
  LCI_OFI_CS_EXIT(LCI_NET_TRYLOCK_SEND);
  if (ret == FI_SUCCESS)
    return errorcode_t::posted;
  else if (ret == -FI_EAGAIN)
    return errorcode_t::retry_nomem;
  else {
    FI_SAFECALL_RET(ret);
  }
}

inline error_t ofi_endpoint_impl_t::post_get_impl(int rank, void* buffer,
                                                  size_t size, mr_t mr,

                                                  uint64_t offset, rmr_t rmr,
                                                  void* user_context,
                                                  bool /*high_priority*/)
{
  uintptr_t addr =
      ofi_detail::get_remote_addr(rmr, offset, ofi_domain_attr->mr_mode);
  struct fi_msg_rma msg;
  struct iovec iov;
  struct fi_rma_iov riov;
  void* desc = ofi_detail::get_mr_desc(mr);
  iov.iov_base = buffer;
  iov.iov_len = size;
  msg.msg_iov = &iov;
  msg.desc = &desc;
  msg.iov_count = 1;
  msg.addr = peer_addrs[rank];
  riov.addr = addr;
  riov.len = size;
  riov.key = rmr.opaque_rkey;
  msg.rma_iov = &riov;
  msg.rma_iov_count = 1;
  msg.data = 0;
  LCI_OFI_CS_TRY_ENTER(LCI_NET_TRYLOCK_SEND, errorcode_t::retry_lock);
  void* ctx = p_ofi_device->wrap_context(user_context, net_opcode_t::READ);
  msg.context = ctx;
  ssize_t ret = fi_readmsg(ofi_ep, &msg, FI_COMPLETION);
  // ssize_t ret = fi_read(ofi_ep, buffer, size, desc, peer_addrs[rank], addr,
  // rkey, user_context);
  if (ret != FI_SUCCESS) p_ofi_device->discard_context(ctx);
  LCI_OFI_CS_EXIT(LCI_NET_TRYLOCK_SEND);
  if (ret == FI_SUCCESS)
    return errorcode_t::posted;
  else if (ret == -FI_EAGAIN)
    return errorcode_t::retry_nomem;
  else {
    FI_SAFECALL_RET(ret);
  }
}
}  // namespace lci

#endif  // LCI_BACKEND_OFI_BACKEND_OFI_INLINE_HPP
