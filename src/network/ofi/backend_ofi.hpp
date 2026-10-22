// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#ifndef LCI_BACKEND_OFI_BACKEND_OFI_HPP
#define LCI_BACKEND_OFI_BACKEND_OFI_HPP

#include <rdma/fabric.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_tagged.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_rma.h>
#include <mutex>

#define FI_SAFECALL(x)                                                    \
  do {                                                                    \
    int err = (x);                                                        \
    if (err < 0) err = -err;                                              \
    if (err) {                                                            \
      LCI_Assert(false, "err : %s (%s:%d)\n", fi_strerror(err), __FILE__, \
                 __LINE__);                                               \
    }                                                                     \
  } while (0)

#define FI_SAFECALL_RET(x)                                                \
  do {                                                                    \
    int err = (x);                                                        \
    if (err < 0) err = -err;                                              \
    if (err) {                                                            \
      LCI_Assert(false, "err : %s (%s:%d)\n", fi_strerror(err), __FILE__, \
                 __LINE__);                                               \
    }                                                                     \
    return errorcode_t::fatal;                                            \
  } while (0)

#define LCI_OFI_CS_TRY_ENTER(mode, ret) \
  if (ofi_lock_mode & mode && !lock.try_lock()) return ret;

#define LCI_OFI_CS_EXIT(mode) \
  if (ofi_lock_mode & mode) lock.unlock();

namespace lci
{
namespace ofi_detail
{
// Providers that require FI_CONTEXT (native psm2 and psm3) own the memory an
// operation's context points at until it completes, so LCI's own context is
// carried in a separate slot the provider may scribble over. The slot also
// records the operation, because psm2's completion flags do not say it (a
// receive completes with FI_REMOTE_CQ_DATA alone, no FI_RECV).
struct op_ctx_t {
  struct fi_context fi_ctx;
  void* user_context;
  net_opcode_t opcode;
};

inline op_ctx_t* alloc_op_ctx(void* user_context, net_opcode_t opcode)
{
  return new op_ctx_t{{}, user_context, opcode};
}

inline void free_op_ctx(op_ctx_t* ctx) { delete ctx; }
}  // namespace ofi_detail

class ofi_lock_guard_t
{
 public:
  ofi_lock_guard_t(uint64_t lock_mode, uint64_t mode, spinlock_t& spinlock)
      : should_lock((lock_mode & mode) != 0), spinlock(spinlock)
  {
    if (should_lock) {
      spinlock.lock();
    }
  }

  ~ofi_lock_guard_t()
  {
    if (should_lock) {
      spinlock.unlock();
    }
  }

  ofi_lock_guard_t(const ofi_lock_guard_t&) = delete;
  ofi_lock_guard_t& operator=(const ofi_lock_guard_t&) = delete;

 private:
  bool should_lock;
  spinlock_t& spinlock;
};

class ofi_net_context_impl_t : public lci::net_context_impl_t
{
 public:
  static bool check_availability();

  ofi_net_context_impl_t(runtime_t runtime_, attr_t attr_);
  ~ofi_net_context_impl_t() override;
  device_t alloc_device(device_t::attr_t attr) override;

  struct fi_info* ofi_info;
  struct fid_fabric* ofi_fabric;
  // The provider requires FI_CONTEXT (see ofi_detail::op_ctx_t)
  bool use_op_ctx;
  // CQ data is too small to carry the source rank next to the 32-bit
  // immediate, so the rank comes from FI_SOURCE (with an FI_AV_TABLE, the
  // source fi_addr is the rank)
  bool rank_from_src;
};

class ofi_device_impl_t : public lci::device_impl_t
{
 public:
  static std::atomic<uint64_t> g_next_rdma_key;

  ofi_device_impl_t(net_context_t context_, device_t::attr_t attr_);
  ~ofi_device_impl_t() override;
  endpoint_t alloc_endpoint_impl(endpoint_t::attr_t attr) override;
  mr_t register_memory_impl(void* buffer, size_t size) override;
  void deregister_memory_impl(mr_impl_t*) override;
  uint64_t get_rkey(mr_impl_t* mr) override;
  size_t poll_comp_impl(net_status_t* p_statuses, size_t max_polls) override;
  error_t post_recv_impl(void* buffer, size_t size, mr_t mr,
                         void* user_context) override;
  size_t post_recvs_impl(void* buffers[], size_t size, size_t count, mr_t mr,
                         void* usesr_contexts[]) override;

  struct fi_domain_attr* ofi_domain_attr;
  struct fid_domain* ofi_domain;
  struct fid_ep* ofi_ep;
  struct fid_cq* ofi_cq;
  struct fid_av* ofi_av;
  std::vector<fi_addr_t> peer_addrs;
  bool use_cxi_writedata;
  bool use_op_ctx;
  bool rank_from_src;

  void* wrap_context(void* user_context, net_opcode_t opcode)
  {
    return use_op_ctx ? ofi_detail::alloc_op_ctx(user_context, opcode)
                      : user_context;
  }
  // Release a wrapped context whose operation was never posted
  void discard_context(void* ctx)
  {
    if (use_op_ctx)
      ofi_detail::free_op_ctx(static_cast<ofi_detail::op_ctx_t*>(ctx));
  }
  // The LCI context of a completed operation; releases the wrapper
  void* unwrap_context(void* ctx)
  {
    if (!use_op_ctx || ctx == nullptr) return ctx;
    auto* op_ctx = static_cast<ofi_detail::op_ctx_t*>(ctx);
    void* user_context = op_ctx->user_context;
    ofi_detail::free_op_ctx(op_ctx);
    return user_context;
  }
  void fill_status(net_status_t& status, const struct fi_cq_data_entry& entry,
                   fi_addr_t src_addr);
  uint64_t& ofi_lock_mode;
  LCIU_CACHE_PADDING(0);
  spinlock_t lock;
  LCIU_CACHE_PADDING(sizeof(spinlock_t));
};

class ofi_endpoint_impl_t : public lci::endpoint_impl_t
{
 public:
  ofi_endpoint_impl_t(device_t device_, attr_t attr_);
  ~ofi_endpoint_impl_t() override;
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
  // CXI rejects FI_REMOTE_CQ_DATA on fi_writemsg, so isolate use of the
  // dedicated writedata entry points until the generic path is supported.
  error_t cxi_inject_writedata_workaround(int rank, void* buffer, size_t size,
                                          uint64_t offset, rmr_t rmr,
                                          net_imm_data_t imm_data,
                                          void* user_context);
  error_t cxi_writedata_workaround(int rank, void* buffer, size_t size, mr_t mr,
                                   uint64_t offset, rmr_t rmr,
                                   net_imm_data_t imm_data, void* user_context);
  error_t post_get_impl(int rank, void* buffer, size_t size, mr_t mr,
                        uint64_t offset, rmr_t rmr, void* user_context,
                        bool high_priority) override;

  ofi_device_impl_t* p_ofi_device;
  int my_rank;
  // Upper 32 bits of the CQ data: the sender's rank, or 0 with rank_from_src
  uint64_t rank_cq_bits;
  struct fi_domain_attr* ofi_domain_attr;
  struct fid_ep* ofi_ep;
  std::vector<fi_addr_t>& peer_addrs;
  uint64_t& ofi_lock_mode;
  spinlock_t& lock;
};

class ofi_mr_impl_t : public lci::mr_impl_t
{
 public:
  struct fid_mr* ofi_mr;
};

}  // namespace lci

#endif  // LCI_BACKEND_OFI_BACKEND_OFI_HPP
