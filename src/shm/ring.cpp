// Copyright (c) 2026 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include "shm/ring.hpp"

#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>

namespace lci
{
namespace shm
{
static_assert(std::atomic<uint64_t>::is_always_lock_free,
              "the SHM ring requires lock-free 64-bit atomics");

namespace
{
uint64_t allocate_ring_identity()
{
  static std::atomic<uint64_t> next_identity(1);
  uint64_t value = next_identity.fetch_add(1, std::memory_order_relaxed);
  if (value == 0) {
    value = next_identity.fetch_add(1, std::memory_order_relaxed);
  }
  return value;
}

bool is_power_of_two(size_t value)
{
  return value != 0 && (value & (value - 1)) == 0;
}
}  // namespace

struct ring_t::slot_header_t {
  std::atomic<uint64_t> sequence;
  uint32_t payload_size;
  net_imm_data_t imm_data;
  int32_t source_local_rank;
  uint32_t reserved;
};

size_t ring_t::required_size(size_t slot_count, size_t slot_size)
{
  if (!is_power_of_two(slot_count) || slot_count > position_modulus / 2 ||
      slot_size < sizeof(slot_header_t) || slot_size % LCI_CACHE_LINE != 0 ||
      slot_size - sizeof(slot_header_t) >
          std::numeric_limits<uint32_t>::max() ||
      slot_count >
          (std::numeric_limits<size_t>::max() - control_size) / slot_size) {
    return 0;
  }
  return control_size + slot_count * slot_size;
}

uint64_t ring_t::encode_sequence(uint64_t position, slot_state_t state)
{
  return (position << 2) | static_cast<uint64_t>(state);
}

bool ring_t::initialize(void* region, size_t region_size, size_t slot_count,
                        size_t slot_size, uint64_t initial_position)
{
  const size_t required = required_size(slot_count, slot_size);
  if (region == nullptr || required == 0 || region_size < required ||
      initial_position > position_mask ||
      reinterpret_cast<uintptr_t>(region) % LCI_CACHE_LINE != 0) {
    return false;
  }

  auto* bytes = static_cast<unsigned char*>(region);
  new (bytes) std::atomic<uint64_t>(initial_position);
  new (bytes + LCI_CACHE_LINE) std::atomic<uint64_t>(initial_position);
  const size_t slot_mask = slot_count - 1;
  const size_t initial_slot = static_cast<size_t>(initial_position) & slot_mask;
  for (size_t i = 0; i < slot_count; ++i) {
    auto* slot =
        reinterpret_cast<slot_header_t*>(bytes + control_size + i * slot_size);
    const uint64_t delta = (i - initial_slot) & slot_mask;
    const uint64_t position = (initial_position + delta) & position_mask;
    new (&slot->sequence)
        std::atomic<uint64_t>(encode_sequence(position, slot_state_t::free));
    slot->payload_size = 0;
    slot->imm_data = 0;
    slot->source_local_rank = -1;
    slot->reserved = 0;
  }
  return true;
}

ring_t::ring_t(void* region_, size_t region_size_, size_t slot_count_,
               size_t slot_size_, size_t max_cas_attempts_)
    : region(static_cast<unsigned char*>(region_)),
      region_size(region_size_),
      slot_count(slot_count_),
      slot_size(slot_size_),
      max_cas_attempts(max_cas_attempts_),
      identity(allocate_ring_identity())
{
  const size_t required = required_size(slot_count, slot_size);
  valid = region != nullptr && required != 0 && region_size >= required &&
          max_cas_attempts != 0 &&
          reinterpret_cast<uintptr_t>(region) % LCI_CACHE_LINE == 0;
}

bool ring_t::is_consistent_empty() const
{
  if (!valid || producer_position()->load(std::memory_order_acquire) != 0 ||
      consumer_position()->load(std::memory_order_acquire) != 0) {
    return false;
  }
  for (size_t i = 0; i < slot_count; ++i) {
    const auto sequence = slot_at(i)->sequence.load(std::memory_order_acquire);
    if (sequence != encode_sequence(i, slot_state_t::free)) return false;
  }
  return true;
}

size_t ring_t::max_payload_size() const
{
  return valid ? slot_size - sizeof(slot_header_t) : 0;
}

std::atomic<uint64_t>* ring_t::producer_position() const
{
  return reinterpret_cast<std::atomic<uint64_t>*>(region);
}

std::atomic<uint64_t>* ring_t::consumer_position() const
{
  return reinterpret_cast<std::atomic<uint64_t>*>(region + LCI_CACHE_LINE);
}

ring_t::slot_header_t* ring_t::slot_at(uint64_t position) const
{
  const size_t index = static_cast<size_t>(position) & (slot_count - 1);
  return reinterpret_cast<slot_header_t*>(region + control_size +
                                          index * slot_size);
}

uint64_t ring_t::add_position(uint64_t position, uint64_t increment) const
{
  return (position + increment) & position_mask;
}

uint64_t ring_t::previous_generation(uint64_t position) const
{
  return (position - static_cast<uint64_t>(slot_count)) & position_mask;
}

bool ring_t::is_previous_generation_state(uint64_t sequence,
                                          uint64_t position) const
{
  const uint64_t previous = previous_generation(position);
  return sequence == encode_sequence(previous, slot_state_t::free) ||
         sequence == encode_sequence(previous, slot_state_t::reserved) ||
         sequence == encode_sequence(previous, slot_state_t::published) ||
         sequence == encode_sequence(previous, slot_state_t::claimed);
}

error_t ring_t::reserve(send_reservation_t* reservation,
                        before_cas_hook_t before_cas, void* hook_arg)
{
  if (!valid || reservation == nullptr || reservation->ring_identity != 0) {
    return errorcode_t::fatal;
  }

  for (size_t attempt = 0; attempt < max_cas_attempts; ++attempt) {
    uint64_t position = producer_position()->load(std::memory_order_relaxed);
    if (position > position_mask) return errorcode_t::fatal;
    slot_header_t* slot = slot_at(position);
    const uint64_t sequence = slot->sequence.load(std::memory_order_acquire);
    if (sequence != encode_sequence(position, slot_state_t::free)) {
      // A competing producer may have advanced the control word and changed
      // this slot after our position load. That is contention, not corrupt
      // shared state; only validate sequence geometry against a stable head.
      if (producer_position()->load(std::memory_order_relaxed) != position) {
        continue;
      }
      if (is_previous_generation_state(sequence, position)) {
        return errorcode_t::retry_nomem;
      }
      return errorcode_t::fatal;
    }

    if (before_cas != nullptr) before_cas(hook_arg);
    uint64_t expected = position;
    if (producer_position()->compare_exchange_weak(
            expected, add_position(position, 1), std::memory_order_relaxed,
            std::memory_order_relaxed)) {
      slot->sequence.store(encode_sequence(position, slot_state_t::reserved),
                           std::memory_order_release);
      reservation->ring_identity = identity;
      reservation->slot = slot;
      reservation->position = position;
      return errorcode_t::done;
    }
  }
  return errorcode_t::retry_lock;
}

error_t ring_t::publish(send_reservation_t* reservation, int source_local_rank,
                        const void* buffer, size_t size,
                        net_imm_data_t imm_data)
{
  if (!valid || reservation == nullptr ||
      reservation->ring_identity != identity || reservation->slot == nullptr ||
      reservation->slot != slot_at(reservation->position) ||
      source_local_rank < 0 || size > max_payload_size() ||
      (size != 0 && buffer == nullptr)) {
    return errorcode_t::fatal;
  }

  slot_header_t* slot = reservation->slot;
  if (slot->sequence.load(std::memory_order_relaxed) !=
      encode_sequence(reservation->position, slot_state_t::reserved)) {
    throw std::runtime_error("SHM ring reservation state is corrupted");
  }
  slot->payload_size = static_cast<uint32_t>(size);
  slot->imm_data = imm_data;
  slot->source_local_rank = source_local_rank;
  slot->reserved = 0;
  if (size != 0) {
    std::memcpy(reinterpret_cast<unsigned char*>(slot) + sizeof(slot_header_t),
                buffer, size);
  }
  slot->sequence.store(
      encode_sequence(reservation->position, slot_state_t::published),
      std::memory_order_release);
  reservation->ring_identity = 0;
  reservation->slot = nullptr;
  reservation->position = 0;
  return errorcode_t::done;
}

error_t ring_t::post_send(int source_local_rank, const void* buffer,
                          size_t size, net_imm_data_t imm_data)
{
  if (!valid || source_local_rank < 0 || size > max_payload_size() ||
      (size != 0 && buffer == nullptr)) {
    return errorcode_t::fatal;
  }

  send_reservation_t reservation;
  const error_t status = reserve(&reservation);
  if (!status.is_done()) return status;
  return publish(&reservation, source_local_rank, buffer, size, imm_data);
}

bool ring_t::poll(recv_slot_t* out)
{
  if (!valid || out == nullptr || out->ring_identity != 0) return false;

  for (size_t attempt = 0; attempt < max_cas_attempts; ++attempt) {
    uint64_t position = consumer_position()->load(std::memory_order_relaxed);
    if (position > position_mask) {
      throw std::runtime_error("SHM ring consumer position is corrupted");
    }
    slot_header_t* slot = slot_at(position);
    const uint64_t sequence = slot->sequence.load(std::memory_order_acquire);
    const uint64_t published =
        encode_sequence(position, slot_state_t::published);
    if (sequence != published) {
      // As on the producer side, a consumer that won after our position load
      // may already have moved this slot to claimed or its next free state.
      if (consumer_position()->load(std::memory_order_relaxed) != position) {
        continue;
      }
      if (sequence == encode_sequence(position, slot_state_t::free) ||
          sequence == encode_sequence(position, slot_state_t::reserved) ||
          sequence == encode_sequence(previous_generation(position),
                                      slot_state_t::published) ||
          sequence == encode_sequence(previous_generation(position),
                                      slot_state_t::claimed)) {
        return false;
      }
      throw std::runtime_error("SHM ring slot sequence is inconsistent");
    }

    uint64_t expected = position;
    if (consumer_position()->compare_exchange_weak(
            expected, add_position(position, 1), std::memory_order_relaxed,
            std::memory_order_relaxed)) {
      slot->sequence.store(encode_sequence(position, slot_state_t::claimed),
                           std::memory_order_release);
      if (slot->payload_size > max_payload_size() ||
          slot->source_local_rank < 0 || slot->reserved != 0) {
        slot->sequence.store(encode_sequence(add_position(position, slot_count),
                                             slot_state_t::free),
                             std::memory_order_release);
        throw std::runtime_error("SHM ring slot metadata is corrupted");
      }
      out->source_local_rank = slot->source_local_rank;
      out->imm_data = slot->imm_data;
      out->payload =
          reinterpret_cast<unsigned char*>(slot) + sizeof(slot_header_t);
      out->size = slot->payload_size;
      out->ring_identity = identity;
      out->shared_slot = slot;
      out->position = position;
      return true;
    }
  }
  return false;
}

bool ring_t::release(recv_slot_t* view)
{
  if (!valid || view == nullptr || view->ring_identity != identity ||
      view->shared_slot == nullptr ||
      view->shared_slot != slot_at(view->position)) {
    return false;
  }

  auto* slot = static_cast<slot_header_t*>(view->shared_slot);
  if (slot->sequence.load(std::memory_order_relaxed) !=
      encode_sequence(view->position, slot_state_t::claimed)) {
    throw std::runtime_error("SHM ring claimed slot state is corrupted");
  }
  slot->sequence.store(encode_sequence(add_position(view->position, slot_count),
                                       slot_state_t::free),
                       std::memory_order_release);
  view->source_local_rank = -1;
  view->imm_data = 0;
  view->payload = nullptr;
  view->size = 0;
  view->ring_identity = 0;
  view->shared_slot = nullptr;
  view->position = 0;
  return true;
}

}  // namespace shm
}  // namespace lci
