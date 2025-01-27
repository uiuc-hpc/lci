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
      abort();                                                              \
    }                                                                       \
  }                                                                         \
  while (0)                                                                 \
    ;

#define LCIXX_OFI_CS_TRY_ENTER(mode, ret) \
  if (lock_mode & mode && !lock.try_lock()) return ret;

#define LCIXX_OFI_CS_EXIT(mode) \
  if (lock_mode & mode) lock.unlock();

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
  net_endpoint_t alloc_net_endpoint(net_endpoint_t::attr_t attr) override;
  mr_t register_memory(void* buffer, size_t size) override;
  void deregister_memory(mr_t) override;
  rkey_t get_rkey(mr_t mr) override;
  std::vector<net_status_t> poll_comp(int max_polls) override;
  error_t post_recv(void* buffer, size_t size, mr_t mr, void* ctx) override;

  struct fi_domain_attr* ofi_domain_attr;
  struct fid_domain* ofi_domain;
  struct fid_ep* ofi_ep;
  struct fid_cq* ofi_cq;
  struct fid_av* ofi_av;
  std::vector<fi_addr_t> peer_addrs;
  uint64_t& lock_mode;
  spinlock_t lock;
};

class ofi_net_endpoint_impl_t : public lcixx::net_endpoint_impl_t
{
 public:
  ofi_net_endpoint_impl_t(net_device_t device_, attr_t attr_);
  ~ofi_net_endpoint_impl_t() override;
  error_t post_sends(int rank, void* buffer, size_t size,
                     net_imm_data_t imm_data) override;
  error_t post_send(int rank, void* buffer, size_t size, mr_t mr,
                    net_imm_data_t imm_data, void* ctx) override;
  error_t post_puts(int rank, void* buffer, size_t size, uintptr_t base,
                    uint64_t offset, rkey_t rkey) override;
  error_t post_put(int rank, void* buffer, size_t size, mr_t mr, uintptr_t base,
                   uint64_t offset, rkey_t rkey, void* ctx) override;
  error_t post_putImms(int rank, void* buffer, size_t size, uintptr_t base,
                       uint64_t offset, rkey_t rkey,
                       net_imm_data_t imm_data) override;
  error_t post_putImm(int rank, void* buffer, size_t size, mr_t mr,
                      uintptr_t base, uint64_t offset, rkey_t rkey,
                      net_imm_data_t imm_data, void* ctx) override;

  ofi_net_device_impl_t* p_ofi_device;
  int my_rank;
  struct fi_domain_attr* ofi_domain_attr;
  struct fid_ep* ofi_ep;
  std::vector<fi_addr_t>& peer_addrs;
  uint64_t& lock_mode;
  spinlock_t& lock;
};

class ofi_mr_impl_t : public lcixx::mr_impl_t
{
 public:
  struct fid_mr* ofi_mr;
};

}  // namespace lcixx

#endif  // LCIXX_BACKEND_OFI_BACKEND_OFI_HPP