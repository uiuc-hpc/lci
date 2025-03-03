#ifndef LCI_BACKEND_INLINE_HPP
#define LCI_BACKEND_INLINE_HPP

namespace lci
{
inline std::vector<net_status_t> device_impl_t::poll_comp(int max_polls)
{
  auto statuses = poll_comp_impl(max_polls);
  LCI_PCOUNTER_ADD(net_poll_cq_entry_count, statuses.size());
  return statuses;
}

inline error_t device_impl_t::post_recv(void* buffer, size_t size, mr_t mr,
                                        void* ctx)
{
  error_t error = post_recv_impl(buffer, size, mr, ctx);
  if (error.is_retry()) {
    LCI_PCOUNTER_ADD(net_recv_post_retry, 1);
  } else {
    LCI_PCOUNTER_ADD(net_recv_post, 1);
  }
  return error;
}

inline mr_t device_impl_t::register_memory(void* address, size_t size)
{
  mr_t mr = register_memory_impl(address, size);
  mr.p_impl->device = device;
  mr.p_impl->address = address;
  mr.p_impl->size = size;
  return mr;
}

inline void device_impl_t::deregister_memory(mr_impl_t* mr)
{
  deregister_memory_impl(mr);
  mr->address = nullptr;
  mr->size = 0;
}

inline bool device_impl_t::post_recv_packet()
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

  mr = packet_pool.p_impl->get_or_register_mr(device);
  size = packet_pool.p_impl->get_pmessage_size();
  error = post_recv(packet->get_payload_address(), size, mr, packet);
  if (error.is_retry()) {
    packet->put_back();
    goto exit_retry;
  }
  return true;

exit_retry:
  --nrecvs_posted;
  return false;
}

inline void device_impl_t::refill_recvs(bool is_blocking)
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

inline void device_impl_t::bind_packet_pool(packet_pool_t packet_pool_)
{
  packet_pool = packet_pool_;
  packet_pool.p_impl->register_packets(device);
  refill_recvs(true);
}

inline void device_impl_t::unbind_packet_pool()
{
  // if we have been using packet pool, report lost packets
  if (packet_pool.p_impl) {
    packet_pool.p_impl->deregister_packets(device);
    packet_pool.p_impl->report_lost_packets(nrecvs_posted);
    packet_pool.p_impl = nullptr;
  }
}

inline void mr_impl_t::deregister()
{
  device.get_impl()->deregister_memory(this);
}

inline rkey_t mr_impl_t::get_rkey()
{
  return device.get_impl()->get_rkey(this);
}

inline error_t endpoint_impl_t::post_sends(int rank, void* buffer, size_t size,
                                           net_imm_data_t imm_data)
{
  auto error = post_sends_impl(rank, buffer, size, imm_data);
  if (error.is_retry()) {
    LCI_PCOUNTER_ADD(net_send_post_retry, 1);
  } else {
    LCI_PCOUNTER_ADD(net_send_post, 1);
  }
  return error;
}

inline error_t endpoint_impl_t::post_send(int rank, void* buffer, size_t size,
                                          mr_t mr, net_imm_data_t imm_data,
                                          void* ctx)
{
  auto error = post_send_impl(rank, buffer, size, mr, imm_data, ctx);
  if (error.is_retry()) {
    LCI_PCOUNTER_ADD(net_send_post_retry, 1);
  } else {
    LCI_PCOUNTER_ADD(net_send_post, 1);
  }
  return error;
}

inline error_t endpoint_impl_t::post_puts(int rank, void* buffer, size_t size,
                                          uintptr_t base, uint64_t offset,
                                          rkey_t rkey)
{
  auto error = post_puts_impl(rank, buffer, size, base, offset, rkey);
  if (error.is_retry()) {
    LCI_PCOUNTER_ADD(net_send_post_retry, 1);
  } else {
    LCI_PCOUNTER_ADD(net_send_post, 1);
  }
  return error;
}

inline error_t endpoint_impl_t::post_put(int rank, void* buffer, size_t size,
                                         mr_t mr, uintptr_t base,
                                         uint64_t offset, rkey_t rkey,
                                         void* ctx)
{
  auto error = post_put_impl(rank, buffer, size, mr, base, offset, rkey, ctx);
  if (error.is_retry()) {
    LCI_PCOUNTER_ADD(net_send_post_retry, 1);
  } else {
    LCI_PCOUNTER_ADD(net_send_post, 1);
  }
  return error;
}

inline error_t endpoint_impl_t::post_putImms(int rank, void* buffer,
                                             size_t size, uintptr_t base,
                                             uint64_t offset, rkey_t rkey,
                                             net_imm_data_t imm_data)
{
  auto error =
      post_putImms_impl(rank, buffer, size, base, offset, rkey, imm_data);
  if (error.is_retry()) {
    LCI_PCOUNTER_ADD(net_send_post_retry, 1);
  } else {
    LCI_PCOUNTER_ADD(net_send_post, 1);
  }
  return error;
}

inline error_t endpoint_impl_t::post_putImm(int rank, void* buffer, size_t size,
                                            mr_t mr, uintptr_t base,
                                            uint64_t offset, rkey_t rkey,
                                            net_imm_data_t imm_data, void* ctx)
{
  auto error = post_putImm_impl(rank, buffer, size, mr, base, offset, rkey,
                                imm_data, ctx);
  if (error.is_retry()) {
    LCI_PCOUNTER_ADD(net_send_post_retry, 1);
  } else {
    LCI_PCOUNTER_ADD(net_send_post, 1);
  }
  return error;
}

inline error_t endpoint_impl_t::post_get(int rank, void* buffer, size_t size,
                                         mr_t mr, uintptr_t base,
                                         uint64_t offset, rkey_t rkey,
                                         void* ctx)
{
  auto error = post_get_impl(rank, buffer, size, mr, base, offset, rkey, ctx);
  if (error.is_retry()) {
    LCI_PCOUNTER_ADD(net_send_post_retry, 1);
  } else {
    LCI_PCOUNTER_ADD(net_send_post, 1);
  }
  return error;
}

}  // namespace lci

#endif  // LCI_BACKEND_INLINE_HPP