#ifndef LCI_BACKEND_IBV_BACKEND_IBV_HPP
#define LCI_BACKEND_IBV_BACKEND_IBV_HPP

#include "infiniband/verbs.h"

#define IBV_SAFECALL(x)                                                        \
  {                                                                            \
    int err = (x);                                                             \
    if (err) {                                                                 \
      LCI_Assert(false, "err %d : %s (%s:%d)\n", err, strerror(err), __FILE__, \
                 __LINE__);                                                    \
    }                                                                          \
  }                                                                            \
  while (0)

#define IBV_SAFECALL_RET(x)                                                    \
  {                                                                            \
    int err = (x);                                                             \
    if (err) {                                                                 \
      LCI_Assert(false, "err %d : %s (%s:%d)\n", err, strerror(err), __FILE__, \
                 __LINE__);                                                    \
    }                                                                          \
    return errorcode_t::fatal;                                                 \
  }                                                                            \
  while (0)

namespace lci
{
class ibv_net_context_impl_t : public lci::net_context_impl_t
{
 public:
  ibv_net_context_impl_t(runtime_t runtime_, attr_t attr_);
  ~ibv_net_context_impl_t() override;
  device_t alloc_device(device_t::attr_t attr) override;

  struct ibv_device** ib_dev_list;
  struct ibv_device* ib_dev;
  struct ibv_context* ib_context;
  struct ibv_pd* ib_pd;
  struct ibv_device_attr ib_dev_attr;
  struct ibv_device_attr_ex ib_dev_attrx;
  struct ibv_port_attr ib_port_attr;
  uint8_t ib_dev_port;
  struct ibv_mr* ib_odp_mr;
  size_t max_inline;
  union ibv_gid ib_gid;
};

struct alignas(LCI_CACHE_LINE) LCISI_ibv_qp_extra_t {
  spinlock_t lock;
  struct ibv_td* ib_td;
  struct ibv_pd* ib_pd;
  LCIU_CACHE_PADDING(sizeof(spinlock_t) + sizeof(struct ibv_td*) +
                     sizeof(struct ibv_pd*));
};

class ibv_mr_impl_t : public lci::mr_impl_t
{
 public:
  struct ibv_mr* ibv_mr;
};

class ibv_device_impl_t : public lci::device_impl_t
{
 public:
  ibv_device_impl_t(net_context_t context_, device_t::attr_t attr_);
  ~ibv_device_impl_t() override;
  endpoint_t alloc_endpoint(endpoint_t::attr_t attr) override;
  mr_t register_memory_impl(void* buffer, size_t size) override;
  void deregister_memory_impl(mr_impl_t*) override;
  rkey_t get_rkey(mr_impl_t* mr) override;
  std::vector<net_status_t> poll_comp_impl(int max_polls) override;
  error_t post_recv_impl(void* buffer, size_t size, mr_t mr,
                         void* ctx) override;

  // Connections O(N)
  struct ibv_td* ib_td;
  struct ibv_pd* ib_pd;
  struct ibv_cq* ib_cq;
  struct ibv_srq* ib_srq;
  std::vector<struct ibv_qp*> ib_qps;
  std::vector<LCISI_ibv_qp_extra_t> ib_qp_extras;
  // Helper fields.
  int* qp2rank;
  int qp2rank_mod;

  net_context_attr_t net_context_attr;
  ibv_mr_impl_t odp_mr;

  spinlock_t srq_lock;
  spinlock_t cq_lock;
};

class ibv_endpoint_impl_t : public lci::endpoint_impl_t
{
 public:
  ibv_endpoint_impl_t(device_t device_, attr_t attr_);
  ~ibv_endpoint_impl_t() override;
  error_t post_sends_impl(int rank, void* buffer, size_t size,
                          net_imm_data_t imm_data) override;
  error_t post_send_impl(int rank, void* buffer, size_t size, mr_t mr,
                         net_imm_data_t imm_data, void* ctx) override;
  error_t post_puts_impl(int rank, void* buffer, size_t size, uintptr_t base,
                         uint64_t offset, rkey_t rkey) override;
  error_t post_put_impl(int rank, void* buffer, size_t size, mr_t mr,
                        uintptr_t base, uint64_t offset, rkey_t rkey,
                        void* ctx) override;
  error_t post_putImms_impl(int rank, void* buffer, size_t size, uintptr_t base,
                            uint64_t offset, rkey_t rkey,
                            net_imm_data_t imm_data) override;
  error_t post_putImm_impl(int rank, void* buffer, size_t size, mr_t mr,
                           uintptr_t base, uint64_t offset, rkey_t rkey,
                           net_imm_data_t imm_data, void* ctx) override;
  error_t post_get_impl(int rank, void* buffer, size_t size, mr_t mr,
                        uintptr_t base, uint64_t offset, rkey_t rkey,
                        void* ctx) override;

  ibv_device_impl_t* p_ibv_device;
  std::vector<struct ibv_qp*> ib_qps;
  std::vector<LCISI_ibv_qp_extra_t> ib_qp_extras;
  net_context_attr_t net_context_attr;

 private:
  bool try_lock_qp(int rank);
  void unlock_qp(int rank);
};

}  // namespace lci

#endif  // LCI_BACKEND_IBV_BACKEND_IBV_HPP