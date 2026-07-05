// Copyright (c) 2026 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#ifndef LCI_SHM_RING_HPP
#define LCI_SHM_RING_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "lci.hpp"

namespace lci
{
namespace shm
{
// A process-local view of one claimed shared-memory slot. The payload remains
// valid until the view is released back to its ring.
struct recv_slot_t {
  recv_slot_t() = default;
  recv_slot_t(const recv_slot_t&) = delete;
  recv_slot_t& operator=(const recv_slot_t&) = delete;

  int rank = -1;
  net_imm_data_t imm_data = 0;
  const void* payload = nullptr;
  size_t size = 0;

 private:
  friend class ring_t;
  class ring_t* owner = nullptr;
  void* shared_slot = nullptr;
  uint64_t position = 0;
};

// A non-owning view over a receiver-owned fixed-slot MPMC ring. The memory may
// come from mmap/shm_open; initialize() must be called by the owner before the
// region is made visible to peers. Configuration is deliberately kept outside
// the shared region so it can be validated as part of the out-of-band handle.
class ring_t
{
 public:
  static constexpr size_t control_size = 2 * LCI_CACHE_LINE;

  static size_t required_size(size_t slot_count, size_t slot_size);
  static bool initialize(void* region, size_t region_size, size_t slot_count,
                         size_t slot_size);

  ring_t(void* region, size_t region_size, size_t slot_count, size_t slot_size,
         size_t max_cas_attempts = 1);

  bool is_valid() const { return valid; }
  size_t capacity() const { return slot_count; }
  size_t max_payload_size() const;

  // retry_lock and retry_nomem are returned only before a slot is reserved.
  // Once reservation succeeds, this operation always copies, publishes, and
  // returns done.
  error_t post_send(int source_local_rank, const void* buffer, size_t size,
                    net_imm_data_t imm_data);

  // Claims at most one slot. Several caller-owned views may be outstanding at
  // once, including from different consumer threads.
  bool poll(recv_slot_t* out);
  bool release(recv_slot_t* slot);

 private:
  struct slot_header_t;

  std::atomic<uint64_t>* producer_position() const;
  std::atomic<uint64_t>* consumer_position() const;
  slot_header_t* slot_at(uint64_t position) const;

  unsigned char* region = nullptr;
  size_t region_size = 0;
  size_t slot_count = 0;
  size_t slot_size = 0;
  size_t max_cas_attempts = 0;
  bool valid = false;
};

}  // namespace shm
}  // namespace lci

#endif  // LCI_SHM_RING_HPP
