// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#ifndef LCI_BACKEND_OFI_BACKEND_OFI_INLINE_HPP
#define LCI_BACKEND_OFI_BACKEND_OFI_INLINE_HPP

namespace lci
{
inline rkey_t ofi_device_impl_t::get_rkey(mr_impl_t* mr)
{
  ofi_mr_impl_t& p_mr = *static_cast<ofi_mr_impl_t*>(mr);
  return fi_mr_key((struct fid_mr*)(p_mr.ofi_mr));
}

inline size_t ofi_device_impl_t::poll_comp_impl(net_status_t* p_statuses,
                                                size_t max_polls)
{
  struct fi_cq_data_entry fi_entries[LCI_BACKEND_MAX_POLLS];

  LCI_OFI_CS_TRY_ENTER(LCI_NET_TRYLOCK_POLL, 0);
  ssize_t ne = fi_cq_read(ofi_cq, fi_entries, max_polls);
  LCI_OFI_CS_EXIT(LCI_NET_TRYLOCK_POLL);
  if (ne > 0) {
    // Got an entry here
    if (p_statuses) {
      for (int j = 0; j < ne; j++) {
        net_status_t& status = p_statuses[j];
        if (fi_entries[j].flags & FI_RECV) {
          status.opcode = net_opcode_t::RECV;
          status.user_context = fi_entries[j].op_context;
          status.length = fi_entries[j].len;
          status.imm_data = fi_entries[j].data & ((1ULL << 32) - 1);
          status.rank = (int)(fi_entries[j].data >> 32);
        } else if (fi_entries[j].flags & FI_REMOTE_WRITE) {
          status.opcode = net_opcode_t::REMOTE_WRITE;
          status.user_context = NULL;
          status.imm_data = fi_entries[j].data;
        } else if (fi_entries[j].flags & FI_SEND) {
          status.opcode = net_opcode_t::SEND;
          status.user_context = fi_entries[j].op_context;
        } else if (fi_entries[j].flags & FI_WRITE) {
          status.opcode = net_opcode_t::WRITE;
          status.user_context = fi_entries[j].op_context;
        } else {
          LCI_DBG_Assert(fi_entries[j].flags & FI_READ,
                         "Unexpected OFI opcode!\n");
          status.opcode = net_opcode_t::READ;
          status.user_context = fi_entries[j].op_context;
        }
      }
    }
  } else if (ne == -FI_EAGAIN) {
    ne = 0;
  } else {
    struct fi_cq_err_entry error;
    char err_data[64];
    error.err_data = err_data;
    error.err_data_size = sizeof(err_data);
    ssize_t ret_cqerr = fi_cq_readerr(ofi_cq, &error, 0);
    // The error was already consumed, most likely by another thread,
    if (ret_cqerr == -FI_EAGAIN) {
      LCI_Assert(ne == -FI_EAVAIL, "unexpected return error: %s\n",
                 fi_strerror(-ne));
    } else {
      LCI_Assert(false, "Err %d: %s\n", error.err, fi_strerror(error.err));
    }
  }
  return ne;
}

namespace ofi_detail
{
inline void* get_mr_desc(mr_t mr)
{
  return fi_mr_desc(static_cast<ofi_mr_impl_t*>(mr.p_impl)->ofi_mr);
}
}  // namespace ofi_detail

inline error_t ofi_device_impl_t::post_recv_impl(void* buffer, size_t size,
                                                 mr_t mr, void* user_context)
{
  auto mr_desc = fi_mr_desc(static_cast<ofi_mr_impl_t*>(mr.p_impl)->ofi_mr);
  LCI_OFI_CS_TRY_ENTER(LCI_NET_TRYLOCK_RECV, errorcode_t::retry_lock);
  ssize_t ret =
      fi_recv(ofi_ep, buffer, size, mr_desc, FI_ADDR_UNSPEC, user_context);
  LCI_OFI_CS_EXIT(LCI_NET_TRYLOCK_RECV);
  if (ret == FI_SUCCESS)
    return errorcode_t::ok;
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
    error = fi_recv(ofi_ep, buffers[i], size, mr_desc, FI_ADDR_UNSPEC,
                    user_contexts[i]);
    if (error == FI_SUCCESS)
      ++n_posted;
    else
      break;
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
                                                    net_imm_data_t imm_data)
{
  LCI_OFI_CS_TRY_ENTER(LCI_NET_TRYLOCK_SEND, errorcode_t::retry_lock);
  ssize_t ret =
      fi_injectdata(ofi_ep, buffer, size, (uint64_t)my_rank << 32 | imm_data,
                    peer_addrs[rank]);
  LCI_OFI_CS_EXIT(LCI_NET_TRYLOCK_SEND);
  if (ret == FI_SUCCESS)
    return errorcode_t::ok;
  else if (ret == -FI_EAGAIN)
    return errorcode_t::retry_nomem;
  else {
    FI_SAFECALL_RET(ret);
  }
}

inline error_t ofi_endpoint_impl_t::post_send_impl(int rank, void* buffer,
                                                   size_t size, mr_t mr,
                                                   net_imm_data_t imm_data,
                                                   void* user_context)
{
  LCI_OFI_CS_TRY_ENTER(LCI_NET_TRYLOCK_SEND, errorcode_t::retry_lock);
  ssize_t ret = fi_senddata(ofi_ep, buffer, size, ofi_detail::get_mr_desc(mr),
                            (uint64_t)my_rank << 32 | imm_data,
                            peer_addrs[rank], user_context);
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
                                                   size_t size, uintptr_t base,
                                                   uint64_t offset, rkey_t rkey)
{
  uintptr_t addr;
  if (ofi_domain_attr->mr_mode & FI_MR_VIRT_ADDR ||
      ofi_domain_attr->mr_mode & FI_MR_BASIC) {
    addr = base + offset;
  } else {
    addr = offset;
  }
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
  riov.key = rkey;
  msg.rma_iov = &riov;
  msg.rma_iov_count = 1;
  msg.context = nullptr;
  msg.data = 0;
  LCI_OFI_CS_TRY_ENTER(LCI_NET_TRYLOCK_SEND, errorcode_t::retry_lock);
  ssize_t ret = fi_writemsg(ofi_ep, &msg, FI_INJECT | FI_DELIVERY_COMPLETE);
  LCI_OFI_CS_EXIT(LCI_NET_TRYLOCK_SEND);
  if (ret == FI_SUCCESS)
    return errorcode_t::ok;
  else if (ret == -FI_EAGAIN)
    return errorcode_t::retry_nomem;
  else {
    FI_SAFECALL_RET(ret);
  }
}

inline error_t ofi_endpoint_impl_t::post_put_impl(int rank, void* buffer,
                                                  size_t size, mr_t mr,
                                                  uintptr_t base,
                                                  uint64_t offset, rkey_t rkey,
                                                  void* user_context)
{
  uintptr_t addr;
  if (ofi_domain_attr->mr_mode & FI_MR_VIRT_ADDR ||
      ofi_domain_attr->mr_mode & FI_MR_BASIC) {
    addr = base + offset;
  } else {
    addr = offset;
  }
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
  riov.key = rkey;
  msg.rma_iov = &riov;
  msg.rma_iov_count = 1;
  msg.context = user_context;
  msg.data = 0;
  LCI_OFI_CS_TRY_ENTER(LCI_NET_TRYLOCK_SEND, errorcode_t::retry_lock);
  ssize_t ret = fi_writemsg(ofi_ep, &msg, FI_DELIVERY_COMPLETE);
  LCI_OFI_CS_EXIT(LCI_NET_TRYLOCK_SEND);
  if (ret == FI_SUCCESS)
    return errorcode_t::posted;
  else if (ret == -FI_EAGAIN)
    return errorcode_t::retry_nomem;
  else {
    FI_SAFECALL_RET(ret);
  }
}

inline error_t ofi_endpoint_impl_t::post_putImms_impl(
    int rank, void* buffer, size_t size, uintptr_t base, uint64_t offset,
    rkey_t rkey, net_imm_data_t imm_data)
{
  uintptr_t addr;
  if (ofi_domain_attr->mr_mode & FI_MR_VIRT_ADDR ||
      ofi_domain_attr->mr_mode & FI_MR_BASIC) {
    addr = base + offset;
  } else {
    addr = offset;
  }
  LCI_OFI_CS_TRY_ENTER(LCI_NET_TRYLOCK_SEND, errorcode_t::retry_lock);
  ssize_t ret = fi_inject_writedata(ofi_ep, buffer, size, imm_data,
                                    peer_addrs[rank], addr, rkey);
  LCI_OFI_CS_EXIT(LCI_NET_TRYLOCK_SEND);
  if (ret == FI_SUCCESS)
    return errorcode_t::ok;
  else if (ret == -FI_EAGAIN)
    return errorcode_t::retry_nomem;
  else {
    FI_SAFECALL_RET(ret);
  }
}

inline error_t ofi_endpoint_impl_t::post_putImm_impl(
    int rank, void* buffer, size_t size, mr_t mr, uintptr_t base,
    uint64_t offset, rkey_t rkey, net_imm_data_t imm_data, void* user_context)
{
  uintptr_t addr;
  if (ofi_domain_attr->mr_mode & FI_MR_VIRT_ADDR ||
      ofi_domain_attr->mr_mode & FI_MR_BASIC) {
    addr = base + offset;
  } else {
    addr = offset;
  }
  LCI_OFI_CS_TRY_ENTER(LCI_NET_TRYLOCK_SEND, errorcode_t::retry_lock);
  ssize_t ret =
      fi_writedata(ofi_ep, buffer, size, ofi_detail::get_mr_desc(mr), imm_data,
                   peer_addrs[rank], addr, rkey, user_context);
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
                                                  uintptr_t base,
                                                  uint64_t offset, rkey_t rkey,
                                                  void* user_context)
{
  uintptr_t addr;
  if (ofi_domain_attr->mr_mode & FI_MR_VIRT_ADDR ||
      ofi_domain_attr->mr_mode & FI_MR_BASIC) {
    addr = base + offset;
  } else {
    addr = offset;
  }
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
  riov.key = rkey;
  msg.rma_iov = &riov;
  msg.rma_iov_count = 1;
  msg.context = user_context;
  msg.data = 0;
  LCI_OFI_CS_TRY_ENTER(LCI_NET_TRYLOCK_SEND, errorcode_t::retry_lock);
  ssize_t ret = fi_readmsg(ofi_ep, &msg, FI_COMPLETION);
  // ssize_t ret = fi_read(ofi_ep, buffer, size, desc, peer_addrs[rank], addr,
  // rkey, user_context);
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