// Copyright (c) 2026 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include "shm/ring.hpp"

#include <cstring>
#include <limits>
#include <new>

namespace lci
{
namespace shm
{
static_assert(std::atomic<uint64_t>::is_always_lock_free,
              "the SHM ring requires lock-free 64-bit atomics");

struct ring_t::slot_header_t {
  std::atomic<uint64_t> sequence;
  uint32_t payload_size;
  net_imm_data_t imm_data;
  int32_t source_local_rank;
  uint32_t reserved;
};

size_t ring_t::required_size(size_t slot_count, size_t slot_size)
{
  if (slot_count == 0 || slot_size < sizeof(slot_header_t) ||
      slot_size % LCI_CACHE_LINE != 0 ||
      slot_size - sizeof(slot_header_t) >
          std::numeric_limits<uint32_t>::max() ||
      slot_count >
          (std::numeric_limits<size_t>::max() - control_size) / slot_size) {
    return 0;
  }
  return control_size + slot_count * slot_size;
}

bool ring_t::initialize(void* region, size_t region_size, size_t slot_count,
                        size_t slot_size)
{
  const size_t required = required_size(slot_count, slot_size);
  if (region == nullptr || required == 0 || region_size < required ||
      reinterpret_cast<uintptr_t>(region) % LCI_CACHE_LINE != 0) {
    return false;
  }

  auto* bytes = static_cast<unsigned char*>(region);
  new (bytes) std::atomic<uint64_t>(0);
  new (bytes + LCI_CACHE_LINE) std::atomic<uint64_t>(0);
  for (size_t i = 0; i < slot_count; ++i) {
    auto* slot =
        reinterpret_cast<slot_header_t*>(bytes + control_size + i * slot_size);
    new (&slot->sequence) std::atomic<uint64_t>(i);
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
      max_cas_attempts(max_cas_attempts_)
{
  const size_t required = required_size(slot_count, slot_size);
  valid = region != nullptr && required != 0 && region_size >= required &&
          max_cas_attempts != 0 &&
          reinterpret_cast<uintptr_t>(region) % LCI_CACHE_LINE == 0;
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
  return reinterpret_cast<slot_header_t*>(region + control_size +
                                          (position % slot_count) * slot_size);
}

error_t ring_t::post_send(int source_local_rank, const void* buffer,
                          size_t size, net_imm_data_t imm_data)
{
  if (!valid || size > max_payload_size() || (size != 0 && buffer == nullptr)) {
    return errorcode_t::fatal;
  }

  uint64_t position = 0;
  slot_header_t* slot = nullptr;
  for (size_t attempt = 0; attempt < max_cas_attempts; ++attempt) {
    position = producer_position()->load(std::memory_order_relaxed);
    slot = slot_at(position);
    if (slot->sequence.load(std::memory_order_acquire) != position) {
      return errorcode_t::retry_nomem;
    }
    if (producer_position()->compare_exchange_weak(position, position + 1,
                                                   std::memory_order_relaxed,
                                                   std::memory_order_relaxed)) {
      slot->payload_size = static_cast<uint32_t>(size);
      slot->imm_data = imm_data;
      slot->source_local_rank = source_local_rank;
      slot->reserved = 0;
      if (size != 0) {
        std::memcpy(
            reinterpret_cast<unsigned char*>(slot) + sizeof(slot_header_t),
            buffer, size);
      }
      slot->sequence.store(position + 1, std::memory_order_release);
      return errorcode_t::done;
    }
  }
  return errorcode_t::retry_lock;
}

bool ring_t::poll(recv_slot_t* out)
{
  if (!valid || out == nullptr || out->owner != nullptr) {
    return false;
  }

  uint64_t position = 0;
  slot_header_t* slot = nullptr;
  for (size_t attempt = 0; attempt < max_cas_attempts; ++attempt) {
    position = consumer_position()->load(std::memory_order_relaxed);
    slot = slot_at(position);
    if (slot->sequence.load(std::memory_order_acquire) != position + 1) {
      return false;
    }
    if (consumer_position()->compare_exchange_weak(position, position + 1,
                                                   std::memory_order_relaxed,
                                                   std::memory_order_relaxed)) {
      if (slot->payload_size > max_payload_size()) {
        // A claimed slot cannot be reported as an ordinary empty poll: doing
        // so would strand it forever. Release it, then report the internal
        // consistency failure through LCI's fatal-error convention.
        slot->sequence.store(position + slot_count, std::memory_order_release);
        throw std::runtime_error("SHM ring slot has an invalid payload size");
      }
      out->rank = slot->source_local_rank;
      out->imm_data = slot->imm_data;
      out->payload =
          reinterpret_cast<unsigned char*>(slot) + sizeof(slot_header_t);
      out->size = slot->payload_size;
      out->owner = this;
      out->shared_slot = slot;
      out->position = position;
      return true;
    }
  }
  return false;
}

bool ring_t::release(recv_slot_t* view)
{
  if (!valid || view == nullptr || view->owner != this ||
      view->shared_slot == nullptr) {
    return false;
  }

  auto* slot = static_cast<slot_header_t*>(view->shared_slot);
  if (slot->sequence.load(std::memory_order_relaxed) != view->position + 1) {
    return false;
  }
  slot->sequence.store(view->position + slot_count, std::memory_order_release);
  view->rank = -1;
  view->imm_data = 0;
  view->payload = nullptr;
  view->size = 0;
  view->owner = nullptr;
  view->shared_slot = nullptr;
  view->position = 0;
  return true;
}

}  // namespace shm
}  // namespace lci
