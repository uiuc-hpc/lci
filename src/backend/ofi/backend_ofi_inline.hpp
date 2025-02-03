#ifndef LCI_BACKEND_OFI_BACKEND_OFI_INLINE_HPP
#define LCI_BACKEND_OFI_BACKEND_OFI_INLINE_HPP

namespace lci
{
inline rkey_t ofi_net_device_impl_t::get_rkey(mr_t mr)
{
  ofi_mr_impl_t& p_mr = *static_cast<ofi_mr_impl_t*>(mr.p_impl);
  return fi_mr_key((struct fid_mr*)(p_mr.ofi_mr));
}

inline std::vector<net_status_t> ofi_net_device_impl_t::poll_comp_impl(
    int max_polls)
{
  // TODO: Investigate the overhead of using a vector here.
  std::vector<net_status_t> statuses;
  if (max_polls > 0)
    statuses.reserve(max_polls);
  else {
    LCI_Assert(false, "max_polls must be greater than 0\n");
  }
  std::vector<struct fi_cq_data_entry> fi_entry(max_polls);

  LCI_OFI_CS_TRY_ENTER(LCI_NET_TRYLOCK_POLL, statuses);
  ssize_t ne = fi_cq_read(ofi_cq, fi_entry.data(), max_polls);
  LCI_OFI_CS_EXIT(LCI_NET_TRYLOCK_POLL);
  if (ne > 0) {
    // Got an entry here
    for (int j = 0; j < ne; j++) {
      net_status_t status;
      if (fi_entry[j].flags & FI_RECV) {
        status.opcode = net_opcode_t::RECV;
        status.ctx = fi_entry[j].op_context;
        status.length = fi_entry[j].len;
        status.imm_data = fi_entry[j].data & ((1ULL << 32) - 1);
        status.rank = (int)(fi_entry[j].data >> 32);
      } else if (fi_entry[j].flags & FI_REMOTE_WRITE) {
        status.opcode = net_opcode_t::REMOTE_WRITE;
        status.ctx = NULL;
        status.imm_data = fi_entry[j].data;
      } else if (fi_entry[j].flags & FI_SEND) {
        LCI_DBG_Assert(
            fi_entry[j].flags & FI_SEND || fi_entry[j].flags & FI_WRITE,
            "Unexpected OFI opcode!\n");
        status.opcode = net_opcode_t::SEND;
        status.ctx = fi_entry[j].op_context;
      } else {
        LCI_DBG_Assert(fi_entry[j].flags & FI_WRITE,
                       "Unexpected OFI opcode!\n");
        status.opcode = net_opcode_t::WRITE;
        status.ctx = fi_entry[j].op_context;
      }
      statuses.push_back(status);
    }
  } else if (ne == -FI_EAGAIN) {
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
  return statuses;
}

namespace ofi_detail
{
inline void* get_mr_desc(mr_t mr)
{
  return fi_mr_desc(static_cast<ofi_mr_impl_t*>(mr.p_impl)->ofi_mr);
}
}  // namespace ofi_detail

inline error_t ofi_net_device_impl_t::post_recv_impl(void* buffer, size_t size,
                                                     mr_t mr, void* ctx)
{
  auto mr_desc = fi_mr_desc(static_cast<ofi_mr_impl_t*>(mr.p_impl)->ofi_mr);
  LCI_OFI_CS_TRY_ENTER(LCI_NET_TRYLOCK_RECV, errorcode_t::retry_lock);
  ssize_t ret = fi_recv(ofi_ep, buffer, size, mr_desc, FI_ADDR_UNSPEC, ctx);
  LCI_OFI_CS_EXIT(LCI_NET_TRYLOCK_RECV);
  if (ret == FI_SUCCESS)
    return errorcode_t::ok;
  else if (ret == -FI_EAGAIN)
    return errorcode_t::retry_nomem;
  else {
    FI_SAFECALL_RET(ret);
  }
}

inline error_t ofi_net_endpoint_impl_t::post_sends_impl(int rank, void* buffer,
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

inline error_t ofi_net_endpoint_impl_t::post_send_impl(int rank, void* buffer,
                                                       size_t size, mr_t mr,
                                                       net_imm_data_t imm_data,
                                                       void* ctx)
{
  LCI_OFI_CS_TRY_ENTER(LCI_NET_TRYLOCK_SEND, errorcode_t::retry_lock);
  ssize_t ret = fi_senddata(ofi_ep, buffer, size, ofi_detail::get_mr_desc(mr),
                            (uint64_t)my_rank << 32 | imm_data,
                            peer_addrs[rank], (struct fi_context*)ctx);
  LCI_OFI_CS_EXIT(LCI_NET_TRYLOCK_SEND);
  if (ret == FI_SUCCESS)
    return errorcode_t::ok;
  else if (ret == -FI_EAGAIN)
    return errorcode_t::retry_nomem;
  else {
    FI_SAFECALL_RET(ret);
  }
}

inline error_t ofi_net_endpoint_impl_t::post_puts_impl(int rank, void* buffer,
                                                       size_t size,
                                                       uintptr_t base,
                                                       uint64_t offset,
                                                       rkey_t rkey)
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
  msg.context = NULL;
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

inline error_t ofi_net_endpoint_impl_t::post_put_impl(int rank, void* buffer,
                                                      size_t size, mr_t mr,
                                                      uintptr_t base,
                                                      uint64_t offset,
                                                      rkey_t rkey, void* ctx)
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
  msg.context = ctx;
  msg.data = 0;
  LCI_OFI_CS_TRY_ENTER(LCI_NET_TRYLOCK_SEND, errorcode_t::retry_lock);
  ssize_t ret = fi_writemsg(ofi_ep, &msg, FI_DELIVERY_COMPLETE);
  LCI_OFI_CS_EXIT(LCI_NET_TRYLOCK_SEND);
  if (ret == FI_SUCCESS)
    return errorcode_t::ok;
  else if (ret == -FI_EAGAIN)
    return errorcode_t::retry_nomem;
  else {
    FI_SAFECALL_RET(ret);
  }
}

inline error_t ofi_net_endpoint_impl_t::post_putImms_impl(
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

inline error_t ofi_net_endpoint_impl_t::post_putImm_impl(
    int rank, void* buffer, size_t size, mr_t mr, uintptr_t base,
    uint64_t offset, rkey_t rkey, net_imm_data_t imm_data, void* ctx)
{
  uintptr_t addr;
  if (ofi_domain_attr->mr_mode & FI_MR_VIRT_ADDR ||
      ofi_domain_attr->mr_mode & FI_MR_BASIC) {
    addr = base + offset;
  } else {
    addr = offset;
  }
  LCI_OFI_CS_TRY_ENTER(LCI_NET_TRYLOCK_SEND, errorcode_t::retry_lock);
  ssize_t ret = fi_writedata(ofi_ep, buffer, size, ofi_detail::get_mr_desc(mr),
                             imm_data, peer_addrs[rank], addr, rkey, ctx);
  LCI_OFI_CS_EXIT(LCI_NET_TRYLOCK_SEND);
  if (ret == FI_SUCCESS)
    return errorcode_t::ok;
  else if (ret == -FI_EAGAIN)
    return errorcode_t::retry_nomem;
  else {
    FI_SAFECALL_RET(ret);
  }
}
}  // namespace lci

#endif  // LCI_BACKEND_OFI_BACKEND_OFI_INLINE_HPP