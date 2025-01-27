#ifndef LCIXX_BACKEND_OFI_BACKEND_OFI_INLINE_HPP
#define LCIXX_BACKEND_OFI_BACKEND_OFI_INLINE_HPP

namespace lcixx
{
inline rkey_t ofi_net_device_impl_t::get_rkey(mr_t mr)
{
  ofi_mr_impl_t& p_mr = *static_cast<ofi_mr_impl_t*>(mr.p_impl);
  return fi_mr_key((struct fid_mr*)(p_mr.ofi_mr));
}

inline std::vector<net_status_t> ofi_net_device_impl_t::poll_comp(int max_polls)
{
  // Investigate the overhead of using a vector here.
  std::vector<net_status_t> statuses;
  if (max_polls > 0)
    statuses.reserve(max_polls);
  else {
    LCIXX_Assert(false, "max_polls must be greater than 0\n");
  }
  std::vector<struct fi_cq_data_entry> fi_entry(max_polls);

  // LCISI_OFI_CS_TRY_ENTER(endpoint_p, LCI_BACKEND_TRY_LOCK_POLL, 0)
  ssize_t ne = fi_cq_read(ofi_cq, fi_entry.data(), max_polls);
  // LCISI_OFI_CS_EXIT(endpoint_p, LCI_BACKEND_TRY_LOCK_POLL)
  // LCII_PCOUNTER_ADD(net_poll_cq_calls, 1);
  if (ne > 0) {
    // LCII_PCOUNTER_ADD(net_poll_cq_entry_count, ne);
    net_status_t status;
    // Got an entry here
    for (int j = 0; j < ne; j++) {
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
        LCIXX_DBG_Assert(
            fi_entry[j].flags & FI_SEND || fi_entry[j].flags & FI_WRITE,
            "Unexpected OFI opcode!\n");
        status.opcode = net_opcode_t::SEND;
        status.ctx = fi_entry[j].op_context;
      } else {
        LCIXX_DBG_Assert(fi_entry[j].flags & FI_WRITE,
                         "Unexpected OFI opcode!\n");
        status.opcode = net_opcode_t::WRITE;
        status.ctx = fi_entry[j].op_context;
      }
    }
    statuses.push_back(status);
  } else if (ne == -FI_EAGAIN) {
  } else {
    struct fi_cq_err_entry error;
    char err_data[64];
    error.err_data = err_data;
    error.err_data_size = sizeof(err_data);
    ssize_t ret_cqerr = fi_cq_readerr(ofi_cq, &error, 0);
    // The error was already consumed, most likely by another thread,
    if (ret_cqerr == -FI_EAGAIN) {
      LCIXX_Assert(ne == -FI_EAVAIL, "unexpected return error: %s\n",
                   fi_strerror(-ne));
    } else {
      LCIXX_Assert(false, "Err %d: %s\n", error.err, fi_strerror(error.err));
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

inline error_t ofi_net_device_impl_t::post_recv(void* buffer, size_t size,
                                                mr_t mr, void* ctx)
{
  auto mr_desc = fi_mr_desc(static_cast<ofi_mr_impl_t*>(mr.p_impl)->ofi_mr);
  LCIXX_OFI_CS_TRY_ENTER(LCIXX_NET_TRYLOCK_RECV, errorcode_t::retry_lock);
  ssize_t ret = fi_recv(ofi_ep, buffer, size, mr_desc, FI_ADDR_UNSPEC, ctx);
  LCIXX_OFI_CS_EXIT(LCIXX_NET_TRYLOCK_RECV);
  if (ret == FI_SUCCESS)
    return errorcode_t::ok;
  else if (ret == -FI_EAGAIN)
    return errorcode_t::retry_nomem;
  else {
    FI_SAFECALL(ret);
  }
}

inline error_t ofi_net_endpoint_impl_t::post_sends(int rank, void* buffer,
                                                   size_t size,
                                                   net_imm_data_t imm_data)
{
  LCIXX_OFI_CS_TRY_ENTER(LCIXX_NET_TRYLOCK_SEND, errorcode_t::retry_lock);
  ssize_t ret =
      fi_injectdata(ofi_ep, buffer, size, (uint64_t)my_rank << 32 | imm_data,
                    peer_addrs[rank]);
  LCIXX_OFI_CS_EXIT(LCIXX_NET_TRYLOCK_SEND);
  if (ret == FI_SUCCESS)
    return errorcode_t::ok;
  else if (ret == -FI_EAGAIN)
    return errorcode_t::retry_nomem;
  else {
    FI_SAFECALL(ret);
  }
}

inline error_t ofi_net_endpoint_impl_t::post_send(int rank, void* buffer,
                                                  size_t size, mr_t mr,
                                                  net_imm_data_t imm_data,
                                                  void* ctx)
{
  LCIXX_OFI_CS_TRY_ENTER(LCIXX_NET_TRYLOCK_SEND, errorcode_t::retry_lock);
  ssize_t ret = fi_senddata(ofi_ep, buffer, size, ofi_detail::get_mr_desc(mr),
                            (uint64_t)my_rank << 32 | imm_data,
                            peer_addrs[rank], (struct fi_context*)ctx);
  LCIXX_OFI_CS_EXIT(LCIXX_NET_TRYLOCK_SEND);
  if (ret == FI_SUCCESS)
    return errorcode_t::ok;
  else if (ret == -FI_EAGAIN)
    return errorcode_t::retry_nomem;
  else {
    FI_SAFECALL(ret);
  }
}

inline error_t ofi_net_endpoint_impl_t::post_puts(int rank, void* buffer,
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
  msg.context = NULL;
  msg.data = 0;
  LCIXX_OFI_CS_TRY_ENTER(LCIXX_NET_TRYLOCK_SEND, errorcode_t::retry_lock);
  ssize_t ret = fi_writemsg(ofi_ep, &msg, FI_INJECT | FI_DELIVERY_COMPLETE);
  LCIXX_OFI_CS_EXIT(LCIXX_NET_TRYLOCK_SEND);
  if (ret == FI_SUCCESS)
    return errorcode_t::ok;
  else if (ret == -FI_EAGAIN)
    return errorcode_t::retry_nomem;
  else {
    FI_SAFECALL(ret);
  }
}

inline error_t ofi_net_endpoint_impl_t::post_put(int rank, void* buffer,
                                                 size_t size, mr_t mr,
                                                 uintptr_t base,
                                                 uint64_t offset, rkey_t rkey,
                                                 void* ctx)
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
  LCIXX_OFI_CS_TRY_ENTER(LCIXX_NET_TRYLOCK_SEND, errorcode_t::retry_lock);
  ssize_t ret = fi_writemsg(ofi_ep, &msg, FI_DELIVERY_COMPLETE);
  LCIXX_OFI_CS_EXIT(LCIXX_NET_TRYLOCK_SEND);
  if (ret == FI_SUCCESS)
    return errorcode_t::ok;
  else if (ret == -FI_EAGAIN)
    return errorcode_t::retry_nomem;
  else {
    FI_SAFECALL(ret);
  }
}

inline error_t ofi_net_endpoint_impl_t::post_putImms(
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
  LCIXX_OFI_CS_TRY_ENTER(LCIXX_NET_TRYLOCK_SEND, errorcode_t::retry_lock);
  ssize_t ret = fi_inject_writedata(ofi_ep, buffer, size, imm_data,
                                    peer_addrs[rank], addr, rkey);
  LCIXX_OFI_CS_EXIT(LCIXX_NET_TRYLOCK_SEND);
  if (ret == FI_SUCCESS)
    return errorcode_t::ok;
  else if (ret == -FI_EAGAIN)
    return errorcode_t::retry_nomem;
  else {
    FI_SAFECALL(ret);
  }
}

inline error_t ofi_net_endpoint_impl_t::post_putImm(
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
  LCIXX_OFI_CS_TRY_ENTER(LCIXX_NET_TRYLOCK_SEND, errorcode_t::retry_lock);
  ssize_t ret = fi_writedata(ofi_ep, buffer, size, ofi_detail::get_mr_desc(mr),
                             imm_data, peer_addrs[rank], addr, rkey, ctx);
  LCIXX_OFI_CS_EXIT(LCIXX_NET_TRYLOCK_SEND);
  if (ret == FI_SUCCESS)
    return errorcode_t::ok;
  else if (ret == -FI_EAGAIN)
    return errorcode_t::retry_nomem;
  else {
    FI_SAFECALL(ret);
  }
}
}  // namespace lcixx

#endif  // LCIXX_BACKEND_OFI_BACKEND_OFI_INLINE_HPP