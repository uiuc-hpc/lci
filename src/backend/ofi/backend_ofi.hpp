#ifndef LCIXX_BACKEND_OFI_BACKEND_OFI_HPP
#define LCIXX_BACKEND_OFI_BACKEND_OFI_HPP

#include <rdma/fabric.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_tagged.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_rma.h>

#define FI_SAFECALL(x)                                                      \
  {                                                                         \
    int err = (x);                                                          \
    if (err < 0) err = -err;                                                \
    if (err) {                                                              \
      LCIXX_Assert(false, "err : %s (%s:%d)\n", fi_strerror(err), __FILE__, \
                   __LINE__);                                               \
    }                                                                       \
  }                                                                         \
  while (0)                                                                 \
    ;

namespace lcixx
{
class ofi_net_context_impl_t : public lcixx::net_context_impl_t
{
 public:
  ofi_net_context_impl_t(runtime_t runtime_, net_context_t::attr_t attr_);
  ~ofi_net_context_impl_t() override;
  net_device_t alloc_net_device(net_device_t::attr_t attr) override;

  struct fi_info* ofi_info;
  struct fid_fabric* ofi_fabric;
};

class ofi_net_device_impl_t : public lcixx::net_device_impl_t
{
 public:
  static std::atomic<uint64_t> g_next_rdma_key;

  ofi_net_device_impl_t(net_context_t context_, net_device_t::attr_t attr_);
  ~ofi_net_device_impl_t() override;
  mr_t register_memory(void* address, size_t size) override;
  void deregister_memory(mr_t) override;
  rkey_t get_rkey(mr_t mr) override;
  std::vector<net_status_t> poll_comp(int max_polls) override;

  struct fi_domain_attr* ofi_domain_attr;
  struct fid_domain* ofi_domain;
  struct fid_ep* ofi_ep;
  struct fid_cq* ofi_cq;
  struct fid_av* ofi_av;
  std::vector<fi_addr_t> peer_addrs;
};

class ofi_net_endpoint_impl_t : public lcixx::net_endpoint_impl_t
{
 public:
  ofi_net_endpoint_impl_t(net_device_t device_, attr_t attr_);
  ~ofi_net_endpoint_impl_t() override;

  struct fid_ep* ofi_ep;
  std::vector<fi_addr_t> peer_addrs;
};

class ofi_mr_impl_t : public lcixx::mr_impl_t
{
 public:
  struct fid_mr* ofi_mr;
};

inline rkey_t ofi_net_device_impl_t::get_rkey(mr_t mr)
{
  ofi_mr_impl_t& p_mr = *static_cast<ofi_mr_impl_t*>(mr.p_impl);
  rkey_t ret(sizeof(uint64_t));
  auto& ofi_mr = p_mr.ofi_mr;
  uint64_t rkey = fi_mr_key((struct fid_mr*)(ofi_mr));
  std::memcpy(&ret[0], (void*)&rkey, sizeof(uint64_t));
  return ret;
}

inline std::vector<net_status_t> ofi_net_device_impl_t::poll_comp(int max_polls)
{
  // Investigate the overhead of using a vector here.
  std::vector<net_status_t> statuses;
  if (max_polls > 0) statuses.reserve(max_polls);
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

}  // namespace lcixx

#endif  // LCIXX_BACKEND_OFI_BACKEND_OFI_HPP