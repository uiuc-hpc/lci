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

// A process-local view of one claimed shared-memory slot. The payload remains
// valid until the view is released. Its ring must outlive the view.
struct recv_slot_t {
  recv_slot_t() = default;
  recv_slot_t(const recv_slot_t&) = delete;
  recv_slot_t& operator=(const recv_slot_t&) = delete;

  int source_global_rank = -1;
  net_imm_data_t imm_data = 0;
  const void* payload = nullptr;
  size_t size = 0;

 private:
  friend class ring_t;
  void* shared_slot = nullptr;
  uint64_t position = 0;
};

// A stable, process-local, non-owning view over a receiver-owned fixed-slot
// MPMC ring. The memory may come from mmap/shm_open; initialize() must be
// called by the owner before the region is made visible to peers. A ring_t
// cannot be copied or moved, and it must outlive all recv_slot_t views it
// returns.
//
// Positions use 63 monotonic bits. The ring fails explicitly before a
// position or generation would become unencodable. A slot sequence is only a
// generation-tagged reusable/published bit: the producer/consumer position
// CAS owns reservation/claim, so neither transition needs another slot store.
// The state bit keeps reusable and published distinct even when capacity is
// one.
class ring_t
{
 public:
  static constexpr size_t control_size =
      2 * LCI_CACHE_LINE;  // one for producer index, one for consumer index
  static constexpr uint64_t maximum_position = (UINT64_C(1) << 63) - 1;

  static size_t required_size(size_t slot_count, size_t slot_size);
  static size_t payload_capacity(size_t slot_size);
  static void initialize(void* region, size_t slot_count, size_t slot_size);

  ring_t(void* region, size_t slot_count, size_t slot_size,
         size_t producer_cas_attempts = 4, size_t consumer_cas_attempts = 1);
  ring_t(const ring_t&) = delete;
  ring_t& operator=(const ring_t&) = delete;
  ring_t(ring_t&&) = delete;
  ring_t& operator=(ring_t&&) = delete;

  // Lifecycle attachment uses this before traffic starts to reject a mapping
  // that does not contain the exact freshly initialized state.
  bool is_consistent_empty() const;
  size_t capacity() const { return slot_count; }
  size_t max_payload_size() const;
  // Returns an inexpensive upper bound on slots that may be ready to receive.
  // It may include producer-reserved slots that are not published yet.
  size_t recv_available_approx() const;

  // retry_lock and retry_nomem are returned only before a slot is reserved.
  // Once reservation succeeds, this operation always copies, publishes, and
  // returns done.
  error_t post_send(int source_global_rank, const void* buffer, size_t size,
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
    slot_header_t* slot = nullptr;
    uint64_t position = 0;
  };
  // This hook is available only to the narrow test friend below. Production
  // reservation always uses reserve(reservation).
  using before_cas_hook_t = void (*)(void*);

  static uint64_t encode_sequence(uint64_t position, slot_state_t state);
  static void initialize_at(void* region, size_t slot_count, size_t slot_size,
                            uint64_t initial_position);
  uint64_t previous_generation(uint64_t position) const;
  bool is_previous_generation(uint64_t sequence, uint64_t position) const;
  error_t reserve(send_reservation_t* reservation);
  error_t reserve_for_test(send_reservation_t* reservation,
                           before_cas_hook_t before_cas, void* hook_arg);
  error_t reserve_impl(send_reservation_t* reservation,
                       before_cas_hook_t before_cas, void* hook_arg);
  error_t publish(send_reservation_t* reservation, int source_global_rank,
                  const void* buffer, size_t size, net_imm_data_t imm_data);

  std::atomic<uint64_t>* producer_position() const;
  std::atomic<uint64_t>* consumer_position() const;
  slot_header_t* slot_at(uint64_t position) const;

  unsigned char* region = nullptr;
  size_t slot_count = 0;
  size_t slot_size = 0;
  size_t producer_cas_attempts = 0;
  size_t consumer_cas_attempts = 0;
};

}  // namespace shm
}  // namespace lci

#endif  // LCI_SHM_RING_HPP
