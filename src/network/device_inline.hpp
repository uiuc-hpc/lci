// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#ifndef LCI_DEVICE_INLINE_HPP
#define LCI_DEVICE_INLINE_HPP

namespace lci
{
inline size_t device_impl_t::poll_comp(net_status_t* p_statuses,
                                       size_t max_polls)
{
  LCI_Assert(max_polls > 0, "max_polls must be greater than 0\n");
  LCI_Assert(max_polls <= LCI_BACKEND_MAX_POLLS,
             "max_polls must be no larger than %lu\n", LCI_BACKEND_MAX_POLLS);
  size_t ret = poll_comp_impl(p_statuses, max_polls);
  LCI_PCOUNTER_ADD(net_poll_cq_entry_count, ret);
  for (size_t i = 0; i < ret; i++) {
    auto& status = p_statuses[i];
    LCI_DBG_Log(
        LOG_TRACE, "network",
        "poll_comp %lu/%lu opcode %s user_context %p length %lu imm_data %x "
        "rank %d\n",
        i + 1, ret, get_net_opcode_str(status.opcode), status.user_context,
        status.length, status.imm_data, status.rank);
    LCI_UNUSED(status);
  }
  return ret;
}

inline error_t device_impl_t::post_recv(void* buffer, size_t size, mr_t mr,
                                        void* user_context)
{
  error_t error = post_recv_impl(buffer, size, mr, user_context);
  if (error.is_retry()) {
    LCI_PCOUNTER_ADD(net_recv_post_retry, 1);
  } else {
    LCI_PCOUNTER_ADD(net_recv_post, 1);
  }
  LCI_DBG_Log(LOG_TRACE, "network",
              "post_recv buffer %p size %lu mr %p user_context %p return %s\n",
              buffer, size, mr.get_impl(), user_context, error.get_str());
  return error;
}

inline size_t device_impl_t::post_recvs(void* buffers[], size_t size,
                                        size_t count, mr_t mr,
                                        void* user_contexts[])
{
  size_t n = post_recvs_impl(buffers, size, count, mr, user_contexts);
  if (n < count) {
    LCI_PCOUNTER_ADD(net_recv_post_retry, 1);
  }
  if (n > 0) {
    LCI_PCOUNTER_ADD(net_recv_post, n);
  }
  LCI_DBG_Log(LOG_TRACE, "network",
              "post_recvs buffers %p size %lu count %lu mr %p return %lu\n",
              buffers, size, count, mr.get_impl(), n);
  return n;
}

inline mr_t device_impl_t::register_memory(void* address, size_t size)
{
  mr_t mr = register_memory_impl(address, size);
  mr.p_impl->device = device;
  mr.p_impl->address = address;
  mr.p_impl->size = size;
  LCI_DBG_Log(LOG_TRACE, "network",
              "register_memory address %p size %lu return %p\n", address, size,
              mr.get_impl());
  return mr;
}

inline void device_impl_t::deregister_memory(mr_impl_t* mr)
{
  deregister_memory_impl(mr);
  LCI_DBG_Log(LOG_TRACE, "network", "deregister_memory mr %p\n", mr);
}

inline bool device_impl_t::post_recv_packets()
{
  mr_t mr;
  size_t size;
  error_t error;
  if (nrecvs_posted >= attr.net_max_recvs) {
    return false;
  }
  const size_t BATCH_SIZE = LCI_BACKEND_MAX_POLLS;

  size_t my_position = nrecvs_posted.fetch_add(BATCH_SIZE);

  if (my_position >= attr.net_max_recvs) {
    nrecvs_posted -= BATCH_SIZE;
    return false;
  }
  size_t nslots = std::min(attr.net_max_recvs - my_position, BATCH_SIZE);
  packet_t* packets[BATCH_SIZE];

  size_t n_popped = packet_pool.p_impl->get_n(nslots, packets, false);
  if (n_popped == 0) {
    nrecvs_posted -= BATCH_SIZE;
    return false;
  }

  mr = packet_pool.p_impl->get_or_register_mr(device);
  size = packet_pool.p_impl->get_payload_size();
  void* buffers[BATCH_SIZE];
  for (size_t i = 0; i < n_popped; i++) {
    buffers[i] = packets[i]->get_payload_address();
  }
  size_t n_posted =
      post_recvs((void**)buffers, size, n_popped, mr, (void**)packets);
  for (size_t i = n_posted; i < n_popped; i++) {
    packets[i]->put_back();
  }
  if (BATCH_SIZE != n_posted) {
    nrecvs_posted -= (BATCH_SIZE - n_posted);
  }
  return n_posted > 0;
}

inline void device_impl_t::refill_recvs(bool is_blocking)
{
  const double refill_threshold = 0.8;
  const int max_retries = 100000;
  int nrecvs_posted = this->nrecvs_posted;
  int niters = 0;
  while (nrecvs_posted < attr.net_max_recvs * refill_threshold) {
    bool succeed = post_recv_packets();
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
    int64_t npackets = packet_pool.get_impl()->get_size();
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

}  // namespace lci

#endif  // LCI_DEVICE_INLINE_HPP