#include "lci_internal.hpp"

namespace lci
{
bool net_device_impl_t::post_recv_packet()
{
  packet_t* packet;
  mr_t mr;
  size_t size;
  error_t error;
  if (nrecvs_posted >= attr.net_max_recvs) {
    return false;
  }
  if (++nrecvs_posted > attr.net_max_recvs) {
    goto exit_retry;
  }
  packet = packet_pool.p_impl->get();
  if (!packet) {
    goto exit_retry;
  }

  mr = packet_pool.p_impl->get_or_register_mr(net_device);
  size = packet_pool.p_impl->get_pmessage_size();
  error = post_recv(packet->get_message_address(), size, mr, packet);
  if (error.is_retry()) {
    packet->put_back();
    goto exit_retry;
  }
  return true;

exit_retry:
  --nrecvs_posted;
  return false;
}

void net_device_impl_t::refill_recvs(bool is_blocking)
{
  // TODO: post multiple receives at the same time to alleviate the atomic
  // overhead
  const double refill_threshold = 0.8;
  const int max_retries = 100000;
  int nrecvs_posted = this->nrecvs_posted;
  int niters = 0;
  while (nrecvs_posted < attr.net_max_recvs * refill_threshold) {
    bool succeed = post_recv_packet();
    if (!succeed) {
      if (is_blocking) {
        ++niters;
        if (niters > max_retries) {
          LCI_Warn(
              "Deadlock alert! The device failed to refill the recvs to the "
              "maximum (current %d)\n",
              nrecvs_posted);
          break;
        }
      } else {
        break;
      }
    }
    nrecvs_posted = this->nrecvs_posted;
  }
  if (nrecvs_posted == 0) {
    int64_t npackets = packet_pool.p_impl->pool.size();
    LCI_Warn(
        "Deadlock alert! The device does not have any posted recvs. (current "
        "packet pool size %ld)\n",
        npackets);
  }
}

void net_device_impl_t::bind_packet_pool(packet_pool_t packet_pool_)
{
  packet_pool = packet_pool_;
  packet_pool.p_impl->register_packets(net_device);
  refill_recvs(true);
}

void net_device_impl_t::unbind_packet_pool()
{
  // if we have been using packet pool, report lost packets
  if (packet_pool.p_impl) {
    packet_pool.p_impl->deregister_packets(net_device);
    packet_pool.p_impl->report_lost_packets(nrecvs_posted);
    packet_pool.p_impl = nullptr;
  }
}

net_context_t alloc_net_context_x::call_impl(
    runtime_t runtime, option_backend_t backend, std::string ofi_provider_name,
    int64_t max_msg_size, int ibv_gid_idx, bool ibv_force_gid_auto_select,
    attr_ibv_odp_strategy_t ibv_odp_strategy,
    attr_ibv_td_strategy_t ibv_td_strategy,
    attr_ibv_prefetch_strategy_t ibv_prefetch_strategy,
    void* user_context) const
{
  net_context_t net_context;

  net_context_t::attr_t attr;
  attr.backend = backend;
  attr.ofi_provider_name = ofi_provider_name;
  attr.max_msg_size = max_msg_size;
  attr.user_context = user_context;
  attr.ibv_gid_idx = ibv_gid_idx;
  attr.ibv_force_gid_auto_select = ibv_force_gid_auto_select;
  attr.ibv_odp_strategy = ibv_odp_strategy;
  attr.ibv_td_strategy = ibv_td_strategy;
  attr.ibv_prefetch_strategy = ibv_prefetch_strategy;

  switch (attr.backend) {
    case option_backend_t::none:
      break;
    case option_backend_t::ibv:
#ifdef LCI_BACKEND_ENABLE_IBV
      net_context.p_impl = new ibv_net_context_impl_t(runtime, attr);
#else
      LCI_Assert(false, "IBV backend is not enabled");
#endif
      break;
    case option_backend_t::ofi:
#ifdef LCI_BACKEND_ENABLE_OFI
      net_context.p_impl = new ofi_net_context_impl_t(runtime, attr);
#else
      LCI_Assert(false, "OFI backend is not enabled");
#endif
      break;
    case option_backend_t::ucx:
#ifdef LCI_BACKEND_ENABLE_UCX
      throw std::runtime_error("UCX backend is not implemented\n");
      // ret.p_impl = new ucx_net_context_impl_t(runtime, attr);
#else
      LCI_Assert(false, "UCX backend is not enabled");
#endif
      break;
    default:
      LCI_Assert(false, "Unsupported backend %d", attr.backend);
  }
  return net_context;
}

void free_net_context_x::call_impl(net_context_t* net_context,
                                   runtime_t runtime) const
{
  delete net_context->p_impl;
  net_context->p_impl = nullptr;
}

std::atomic<int> net_device_impl_t::g_ndevices(0);

net_device_t alloc_net_device_x::call_impl(
    runtime_t runtime, int64_t net_max_sends, int64_t net_max_recvs,
    int64_t net_max_cqes, uint64_t ofi_lock_mode, void* user_context,
    net_context_t net_context) const
{
  net_device_t::attr_t attr;
  attr.net_max_sends = net_max_sends;
  attr.net_max_recvs = net_max_recvs;
  attr.net_max_cqes = net_max_cqes;
  attr.ofi_lock_mode = ofi_lock_mode;
  attr.user_context = user_context;
  auto net_device = net_context.p_impl->alloc_net_device(attr);
  packet_pool_t packet_pool = get_default_packet_pool_x().runtime(runtime)();
  if (!packet_pool.is_empty()) {
    bind_packet_pool_x(net_device, packet_pool).runtime(runtime)();
  }
  return net_device;
}

void free_net_device_x::call_impl(net_device_t* net_device,
                                  runtime_t runtime) const
{
  delete net_device->p_impl;
  net_device->p_impl = nullptr;
}

mr_t register_memory_x::call_impl(void* address, size_t size, runtime_t runtime,
                                  net_device_t net_device) const
{
  mr_t mr = net_device.p_impl->register_memory(address, size);
  mr.p_impl->device = net_device;
  return mr;
}

void deregister_memory_x::call_impl(mr_t* mr, runtime_t runtime) const
{
  mr->p_impl->device.p_impl->deregister_memory(*mr);
  mr->p_impl = nullptr;
}

rkey_t get_rkey_x::call_impl(mr_t mr, runtime_t runtime) const
{
  return mr.p_impl->device.p_impl->get_rkey(mr);
}

std::vector<net_status_t> net_poll_cq_x::call_impl(runtime_t runtime,
                                                   net_device_t net_device,
                                                   int max_polls) const
{
  return net_device.p_impl->poll_comp(max_polls);
}

std::atomic<int> net_endpoint_impl_t::g_nendpoints(0);

net_endpoint_t alloc_net_endpoint_x::call_impl(runtime_t runtime,
                                               void* user_context,
                                               net_device_t net_device) const
{
  net_endpoint_t::attr_t attr;
  attr.user_context = user_context;
  auto net_endpoint = net_device.p_impl->alloc_net_endpoint(attr);
  return net_endpoint;
}

void free_net_endpoint_x::call_impl(net_endpoint_t* net_endpoint,
                                    runtime_t runtime) const
{
  delete net_endpoint->p_impl;
  net_endpoint->p_impl = nullptr;
}

error_t net_post_recv_x::call_impl(void* buffer, size_t size, mr_t mr,
                                   runtime_t runtime, net_device_t net_device,
                                   void* ctx) const
{
  return net_device.p_impl->post_recv(buffer, size, mr, ctx);
}

error_t net_post_sends_x::call_impl(int rank, void* buffer, size_t size,
                                    runtime_t runtime,
                                    net_endpoint_t net_endpoint,
                                    net_imm_data_t imm_data) const
{
  return net_endpoint.p_impl->post_sends(rank, buffer, size, imm_data);
}

error_t net_post_send_x::call_impl(int rank, void* buffer, size_t size, mr_t mr,
                                   runtime_t runtime,
                                   net_endpoint_t net_endpoint,
                                   net_imm_data_t imm_data, void* ctx) const
{
  return net_endpoint.p_impl->post_send(rank, buffer, size, mr, imm_data, ctx);
}

error_t net_post_puts_x::call_impl(int rank, void* buffer, size_t size,
                                   uintptr_t base, uint64_t offset, rkey_t rkey,
                                   runtime_t runtime,
                                   net_endpoint_t net_endpoint) const
{
  return net_endpoint.p_impl->post_puts(rank, buffer, size, base, offset, rkey);
}

error_t net_post_put_x::call_impl(int rank, void* buffer, size_t size, mr_t mr,
                                  uintptr_t base, uint64_t offset, rkey_t rkey,
                                  runtime_t runtime,
                                  net_endpoint_t net_endpoint, void* ctx) const
{
  return net_endpoint.p_impl->post_put(rank, buffer, size, mr, base, offset,
                                       rkey, ctx);
}

error_t net_post_putImms_x::call_impl(int rank, void* buffer, size_t size,
                                      uintptr_t base, uint64_t offset,
                                      rkey_t rkey, runtime_t runtime,
                                      net_endpoint_t net_endpoint,
                                      net_imm_data_t imm_data) const
{
  return net_endpoint.p_impl->post_putImms(rank, buffer, size, base, offset,
                                           rkey, imm_data);
}

error_t net_post_putImm_x::call_impl(int rank, void* buffer, size_t size,
                                     mr_t mr, uintptr_t base, uint64_t offset,
                                     rkey_t rkey, runtime_t runtime,
                                     net_endpoint_t net_endpoint,
                                     net_imm_data_t imm_data, void* ctx) const
{
  return net_endpoint.p_impl->post_putImm(rank, buffer, size, mr, base, offset,
                                          rkey, imm_data, ctx);
}
}  // namespace lci