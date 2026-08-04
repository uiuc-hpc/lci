// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

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
    [[maybe_unused]] auto& status = p_statuses[i];
    LCI_DBG_Log(
        LOG_TRACE, "network",
        "poll_comp %lu/%lu opcode %s user_context %p length %lu imm_data %x "
        "rank %d\n",
        i + 1, ret, get_net_opcode_str(status.opcode), status.user_context,
        status.length, status.imm_data, status.rank);
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
  mr_t mr;
  // reg cache does not like size 0
  if (attr.use_reg_cache && rcache_handle && rcache_handle->is_valid() &&
      size > 0) {
    mr = rcache_handle->get(address, size);
  } else {
    mr = register_memory_impl(address, size);
    mr.p_impl->device = device;
    mr.p_impl->address = address;
    mr.p_impl->size = size;
    mr.p_impl->mr_base = address;
  }
  LCI_DBG_Log(LOG_TRACE, "network",
              "register_memory address %p size %lu return %p\n", address, size,
              mr.get_impl());
  return mr;
}

inline void device_impl_t::deregister_memory(mr_impl_t* mr)
{
  LCI_DBG_Log(LOG_TRACE, "network", "deregister_memory mr %p\n", mr);
  if (attr.use_reg_cache && rcache_handle && rcache_handle->is_valid() &&
      mr->size > 0) {
    rcache_handle->put(mr);
  } else {
    deregister_memory_impl(mr);
  }
}

inline void device_impl_t::destroy_reg_cache()
{
  if (rcache_handle) {
    delete rcache_handle;
    rcache_handle = nullptr;
  }
}

inline void device_impl_t::release_posted_recv_slot(packet_t* packet)
{
  const size_t slot = packet->local_context.posted_recv_slot;
  LCI_Assert(slot != PACKET_POSTED_RECV_SLOT_INVALID &&
                 slot < posted_recvs.size() && posted_recvs[slot] == packet,
             "Invalid posted receive slot\n");
  posted_recvs[slot] = nullptr;
  free_posted_recv_slots.push_back(slot);
  packet->local_context.posted_recv_slot = PACKET_POSTED_RECV_SLOT_INVALID;
}

inline packet_t* device_impl_t::complete_recv(void* user_context)
{
  std::lock_guard<spinlock_t> lock(posted_recvs_lock);
  if (!packet_pool.p_impl ||
      !packet_pool.p_impl->is_packet(user_context, true)) {
    return nullptr;
  }

  packet_t* packet = static_cast<packet_t*>(user_context);
  const size_t slot = packet->local_context.posted_recv_slot;
  if (slot == PACKET_POSTED_RECV_SLOT_INVALID || slot >= posted_recvs.size() ||
      posted_recvs[slot] != packet) {
    return nullptr;
  }

  release_posted_recv_slot(packet);
  const size_t previous = nrecvs_posted.fetch_sub(1, std::memory_order_relaxed);
  LCI_Assert(previous > 0, "Posted receive count underflow\n");
  return packet;
}

inline bool device_impl_t::post_recv_packets()
{
  const size_t BATCH_SIZE = LCI_BACKEND_MAX_POLLS * 2;
  packet_t* packets[BATCH_SIZE];
  void* buffers[BATCH_SIZE];

  std::lock_guard<spinlock_t> lock(posted_recvs_lock);
  if (posted_recvs_stopping || !packet_pool.p_impl ||
      free_posted_recv_slots.empty()) {
    return false;
  }

  const size_t nslots = std::min(BATCH_SIZE, free_posted_recv_slots.size());
  size_t n_popped = packet_pool.p_impl->get_n(nslots, packets, false);
  if (n_popped == 0) {
    return false;
  }

  for (size_t i = 0; i < n_popped; i++) {
    buffers[i] = packets[i]->get_payload_address();
    const size_t slot = free_posted_recv_slots.back();
    free_posted_recv_slots.pop_back();
    posted_recvs[slot] = packets[i];
    packets[i]->local_context.posted_recv_slot = slot;
  }

  mr_t mr = packet_pool.p_impl->get_or_register_mr(device);
  size_t size = packet_pool.p_impl->get_payload_size();
  size_t n_posted =
      post_recvs((void**)buffers, size, n_popped, mr, (void**)packets);
  LCI_Assert(n_posted <= n_popped,
             "Backend posted more receives than requested\n");
  for (size_t i = n_posted; i < n_popped; i++) {
    release_posted_recv_slot(packets[i]);
    packets[i]->put_back();
  }
  if (n_posted > 0) {
    nrecvs_posted.fetch_add(n_posted, std::memory_order_relaxed);
  }
  return n_posted > 0;
}

inline bool device_impl_t::refill_recvs(bool is_blocking)
{
  if (!packet_pool.p_impl) {
    return false;
  }
  const double refill_threshold = 0.8;
  const int max_retries = 100000;
  bool ret = false;
  size_t nrecvs_posted = this->nrecvs_posted;
  int niters = 0;
  while (nrecvs_posted < attr.net_max_recvs * refill_threshold) {
    bool succeed = post_recv_packets();
    if (!succeed) {
      if (is_blocking) {
        ++niters;
        if (niters > max_retries) {
          LCI_Warn(
              "Deadlock alert! The device failed to refill the recvs to the "
              "maximum (current %lu)\n",
              nrecvs_posted);
          break;
        }
      } else {
        break;
      }
    } else {
      // succeeded
      ret = true;
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
  return ret;
}

inline void device_impl_t::bind_packet_pool(packet_pool_t packet_pool_)
{
  {
    std::lock_guard<spinlock_t> lock(posted_recvs_lock);
    LCI_Assert(!packet_pool.p_impl, "A packet pool is already bound\n");
    packet_pool = packet_pool_;
    posted_recvs.assign(attr.net_max_recvs, nullptr);
    free_posted_recv_slots.clear();
    free_posted_recv_slots.reserve(attr.net_max_recvs);
    for (size_t i = attr.net_max_recvs; i > 0; --i) {
      free_posted_recv_slots.push_back(i - 1);
    }
    posted_recvs_stopping = false;
    nrecvs_posted.store(0, std::memory_order_relaxed);
  }
  packet_pool.p_impl->register_packets(device);
  refill_recvs(true);
}

inline void device_impl_t::unbind_packet_pool()
{
  packet_pool_impl_t* p_packet_pool = nullptr;
  std::vector<packet_t*> posted_recvs_snapshot;
  {
    std::lock_guard<spinlock_t> lock(posted_recvs_lock);
    if (!packet_pool.p_impl) {
      return;
    }
    posted_recvs_stopping = true;
    p_packet_pool = packet_pool.p_impl;
    posted_recvs_snapshot = posted_recvs;
  }

  // The endpoint resources that can still reference these packets must be
  // retired before their MR is closed or the packets are returned to the pool.
  quiesce_recvs_impl(posted_recvs_snapshot);
  p_packet_pool->deregister_packets(device);

  {
    std::lock_guard<spinlock_t> lock(posted_recvs_lock);
    for (packet_t* packet : posted_recvs) {
      if (packet != nullptr) {
        packet->local_context.posted_recv_slot =
            PACKET_POSTED_RECV_SLOT_INVALID;
        packet->put_back();
      }
    }
    posted_recvs.clear();
    free_posted_recv_slots.clear();
    nrecvs_posted.store(0, std::memory_order_relaxed);
    packet_pool.p_impl = nullptr;
  }
}

}  // namespace lci

#endif  // LCI_DEVICE_INLINE_HPP
