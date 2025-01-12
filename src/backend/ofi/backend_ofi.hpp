#ifndef LCIXX_BACKEND_OFI_BACKEND_OFI_HPP
#define LCIXX_BACKEND_OFI_BACKEND_OFI_HPP

#include <rdma/fabric.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_tagged.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_rma.h>

#define FI_SAFECALL(x)                                                    \
  {                                                                       \
    int err = (x);                                                        \
    if (err < 0) err = -err;                                              \
    if (err) {                                                            \
      LCIXX_Assert(false, "err : %s (%s:%d)\n", fi_strerror(err), __FILE__, \
                 __LINE__);                                               \
    }                                                                     \
  }                                                                       \
  while (0)                                                               \
    ;


namespace lcixx {
class ofi_net_context_impl_t : public lcixx::net_context_impl_t {
public:
    ofi_net_context_impl_t(runtime_t runtime_, net_context_t::config_t config_);
    ~ofi_net_context_impl_t();
    net_device_t alloc_net_device(net_device_t::config_t config) override {
        net_device_t ret;
        ret.p_impl = std::make_shared<ofi_net_device_impl_t>(runtime, config);
        return ret;
    }

    struct fi_info* ofi_info;
    struct fid_fabric* ofi_fabric;
};

class ofi_net_device_impl_t : public lcixx::net_device_impl_t {
public:
    static std::atomic<uint64_t> g_next_rdma_key;

    ofi_net_device_impl_t(net_context_t context_, net_device_t::config_t config_);
    ~ofi_net_device_impl_t();

    mr_t register_memory(void* address, size_t size) override;
    void deregister_memory(mr_t) override;

    struct fid_domain* ofi_domain;
    struct fi_domain_attr *ofi_domain_attr;
    struct fid_ep* ofi_ep;
    struct fid_cq* ofi_cq;
    struct fid_av* ofi_av;
    std::vector<fi_addr_t> peer_addrs;
};

class ofi_mr_impl_t : public lcixx::mr_impl_t {
public:
  struct fid_mr* ofi_mr; 
};
} // namespace lcixx

#endif // LCIXX_BACKEND_OFI_BACKEND_OFI_HPP