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
class ring_t;
class ring_test_access_t;

// A process-local view of one claimed shared-memory slot. The payload remains
// valid until the view is released back to the same, still-live ring view.
struct recv_slot_t {
  recv_slot_t() = default;
  recv_slot_t(const recv_slot_t&) = delete;
  recv_slot_t& operator=(const recv_slot_t&) = delete;

  int source_local_rank = -1;
  net_imm_data_t imm_data = 0;
  const void* payload = nullptr;
  size_t size = 0;

 private:
  friend class ring_t;
  uint64_t ring_identity = 0;
  void* shared_slot = nullptr;
  uint64_t position = 0;
};

// A stable, process-local, non-owning view over a receiver-owned fixed-slot
// MPMC ring. The memory may come from mmap/shm_open; initialize() must be
// called by the owner before the region is made visible to peers. A ring_t
// cannot be copied or moved, and it must outlive all recv_slot_t views it
// returns.
//
// Positions use 63 bits and wrap modulo 2^63. A slot sequence is only a
// generation-tagged reusable/published bit: the producer/consumer position
// CAS owns reservation/claim, so neither transition needs another slot store.
// Requiring a power-of-two capacity no larger than half the position space
// makes generation comparisons unambiguous across counter wrap. The state bit
// keeps reusable and published distinct even when capacity is one.
class ring_t
{
 public:
  static constexpr size_t control_size = 2 * LCI_CACHE_LINE;
  static constexpr uint64_t position_modulus = UINT64_C(1) << 63;
  static constexpr uint64_t position_mask = position_modulus - 1;

  static size_t required_size(size_t slot_count, size_t slot_size);
  static size_t payload_capacity(size_t slot_size);
  static bool initialize(void* region, size_t region_size, size_t slot_count,
                         size_t slot_size);

  ring_t(void* region, size_t region_size, size_t slot_count, size_t slot_size,
         size_t max_cas_attempts = 1);
  ring_t(const ring_t&) = delete;
  ring_t& operator=(const ring_t&) = delete;
  ring_t(ring_t&&) = delete;
  ring_t& operator=(ring_t&&) = delete;

  bool is_valid() const { return valid; }
  // Lifecycle attachment uses this before traffic starts to reject a mapping
  // that does not contain the exact freshly initialized state.
  bool is_consistent_empty() const;
  size_t capacity() const { return slot_count; }
  size_t max_payload_size() const;

  // retry_lock and retry_nomem are returned only before a slot is reserved.
  // Once reservation succeeds, this operation always copies, publishes, and
  // returns done.
  error_t post_send(int source_local_rank, const void* buffer, size_t size,
                    net_imm_data_t imm_data);

  // Claims at most one slot. Several caller-owned views may be outstanding at
  // once, including from different consumer threads. A malformed shared state
  // is an internal consistency error rather than an ordinary empty result.
  bool poll(recv_slot_t* out);
  bool release(recv_slot_t* slot);

 private:
  friend class ring_test_access_t;

  enum class slot_state_t : uint64_t { reusable = 0, published = 1 };
  struct slot_header_t;
  struct send_reservation_t {
    uint64_t ring_identity = 0;
    slot_header_t* slot = nullptr;
    uint64_t position = 0;
  };
  using before_cas_hook_t = void (*)(void*);

  static uint64_t encode_sequence(uint64_t position, slot_state_t state);
  static bool initialize_at(void* region, size_t region_size, size_t slot_count,
                            size_t slot_size, uint64_t initial_position);
  uint64_t add_position(uint64_t position, uint64_t increment) const;
  uint64_t previous_generation(uint64_t position) const;
  bool is_previous_generation(uint64_t sequence, uint64_t position) const;
  error_t reserve(send_reservation_t* reservation,
                  before_cas_hook_t before_cas = nullptr,
                  void* hook_arg = nullptr);
  error_t publish(send_reservation_t* reservation, int source_local_rank,
                  const void* buffer, size_t size, net_imm_data_t imm_data);

  std::atomic<uint64_t>* producer_position() const;
  std::atomic<uint64_t>* consumer_position() const;
  slot_header_t* slot_at(uint64_t position) const;

  unsigned char* region = nullptr;
  size_t region_size = 0;
  size_t slot_count = 0;
  size_t slot_size = 0;
  size_t max_cas_attempts = 0;
  uint64_t identity = 0;
  bool valid = false;
};

}  // namespace shm
}  // namespace lci

#endif  // LCI_SHM_RING_HPP
