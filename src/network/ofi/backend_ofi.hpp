// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#ifndef LCI_BACKEND_OFI_BACKEND_OFI_HPP
#define LCI_BACKEND_OFI_BACKEND_OFI_HPP

#include <rdma/fabric.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_tagged.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_rma.h>

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
class ofi_net_context_impl_t : public lci::net_context_impl_t
{
 public:
  ofi_net_context_impl_t(runtime_t runtime_, attr_t attr_);
  ~ofi_net_context_impl_t() override;
  device_t alloc_device(device_t::attr_t attr) override;

  struct fi_info* ofi_info;
  struct fid_fabric* ofi_fabric;
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
  rkey_t get_rkey(mr_impl_t* mr) override;
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
                          net_imm_data_t imm_data) override;
  error_t post_send_impl(int rank, void* buffer, size_t size, mr_t mr,
                         net_imm_data_t imm_data, void* user_context) override;
  error_t post_puts_impl(int rank, void* buffer, size_t size, uintptr_t base,
                         uint64_t offset, rkey_t rkey) override;
  error_t post_put_impl(int rank, void* buffer, size_t size, mr_t mr,
                        uintptr_t base, uint64_t offset, rkey_t rkey,
                        void* user_context) override;
  error_t post_putImms_impl(int rank, void* buffer, size_t size, uintptr_t base,
                            uint64_t offset, rkey_t rkey,
                            net_imm_data_t imm_data) override;
  error_t post_putImm_impl(int rank, void* buffer, size_t size, mr_t mr,
                           uintptr_t base, uint64_t offset, rkey_t rkey,
                           net_imm_data_t imm_data,
                           void* user_context) override;
  error_t post_get_impl(int rank, void* buffer, size_t size, mr_t mr,
                        uintptr_t base, uint64_t offset, rkey_t rkey,
                        void* user_context) override;

  ofi_device_impl_t* p_ofi_device;
  int my_rank;
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