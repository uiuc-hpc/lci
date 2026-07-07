// Copyright (c) 2026 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include "lci_internal.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>

namespace lci
{
namespace shm
{
namespace
{
constexpr size_t hostname_capacity = 128;

struct hostname_record_t {
  char hostname[hostname_capacity] = {};
};

bool is_power_of_two(size_t value)
{
  return value != 0 && (value & (value - 1)) == 0;
}

void fill_random(void* data, size_t size)
{
  std::ifstream urandom("/dev/urandom", std::ios::in | std::ios::binary);
  if (urandom) {
    urandom.read(static_cast<char*>(data), static_cast<std::streamsize>(size));
    if (urandom.gcount() == static_cast<std::streamsize>(size)) return;
  }

  uint64_t seed =
      static_cast<uint64_t>(std::chrono::high_resolution_clock::now()
                                .time_since_epoch()
                                .count()) ^
      (static_cast<uint64_t>(getpid()) << 32) ^
      static_cast<uint64_t>(lci::rand_mt());
  auto* out = static_cast<unsigned char*>(data);
  for (size_t i = 0; i < size; ++i) {
    seed = seed * UINT64_C(6364136223846793005) + UINT64_C(1442695040888963407);
    out[i] = static_cast<unsigned char>(seed >> 56);
  }
}

uint64_t random_nonzero_u64()
{
  uint64_t value = 0;
  while (value == 0) fill_random(&value, sizeof(value));
  return value;
}

std::string nonce_to_hex(const std::array<uint64_t, 2>& nonce)
{
  std::ostringstream out;
  out << std::hex << std::setfill('0') << std::setw(16) << nonce[0]
      << std::setw(16) << nonce[1];
  return out.str();
}

std::string make_object_name(context_impl_t* context, uint64_t device_uid,
                             int owner_global_rank)
{
  std::ostringstream out;
  out << "/lci-" << nonce_to_hex(context->job_nonce) << "-" << std::hex
      << std::setfill('0') << std::setw(16) << device_uid << std::dec << "-"
      << owner_global_rank;
  return out.str();
}

posix_ring_expected_t expected_ring(context_impl_t* context,
                                    uint64_t device_uid, int owner_global_rank,
                                    size_t slot_count, size_t slot_size,
                                    size_t max_message_size)
{
  posix_ring_expected_t expected;
  expected.owner_global_rank = owner_global_rank;
  expected.device_uid = device_uid;
  expected.mapping_size = posix_ring_mapping_size(slot_count, slot_size);
  expected.slot_count = slot_count;
  expected.slot_size = slot_size;
  expected.max_message_size = max_message_size;
  expected.name = make_object_name(context, device_uid, owner_global_rank);
  return expected;
}

uint64_t load_counter(const std::atomic<uint64_t>& value)
{
  return value.load(std::memory_order_relaxed);
}

bool match_rank_token(const char* token, int rank)
{
  if (token == nullptr || *token == '\0') return false;
  if (token[0] == '*' && token[1] == '\0') return true;
  char* end = nullptr;
  long parsed = std::strtol(token, &end, 10);
  return end != token && *end == '\0' && parsed == rank;
}

// Test-only fault injection used by the multiprocess fallback tests. The
// comma-separated format is "src:dst" with '*' accepted on either side.
// Example: LCI_SHM_TEST_DISABLE_ROUTE=0:1 disables only rank 0 -> rank 1
// before reciprocal route agreement.
bool test_disables_route(int source_rank, int target_rank)
{
  const char* spec = std::getenv("LCI_SHM_TEST_DISABLE_ROUTE");
  if (spec == nullptr || *spec == '\0') return false;
  const char* item = spec;
  while (*item != '\0') {
    const char* comma = std::strchr(item, ',');
    const size_t item_len = comma == nullptr
                                ? std::strlen(item)
                                : static_cast<size_t>(comma - item);
    const char* colon =
        static_cast<const char*>(std::memchr(item, ':', item_len));
    if (colon != nullptr) {
      char source[32] = {};
      char target[32] = {};
      const size_t source_len = static_cast<size_t>(colon - item);
      const size_t target_len = item_len - source_len - 1;
      if (source_len < sizeof(source) && target_len < sizeof(target)) {
        std::memcpy(source, item, source_len);
        std::memcpy(target, colon + 1, target_len);
        if (match_rank_token(source, source_rank) &&
            match_rank_token(target, target_rank)) {
          return true;
        }
      }
    }
    if (comma == nullptr) break;
    item = comma + 1;
  }
  return false;
}
}  // namespace

class device_impl_t
{
 public:
  context_t context;
  lci::device_t core_device;
  bool enabled = false;
  bool collective_lifecycle = false;
  uint64_t device_uid = 0;
  size_t slot_count = 0;
  size_t slot_size = 0;
  size_t max_message_size = 0;
  size_t max_cas_attempts = 0;
  std::unique_ptr<posix_owner_mapping_t> owner;
  std::vector<std::unique_ptr<posix_peer_mapping_t>> peer_mappings;
  std::vector<ring_t*> routes;
  std::vector<uint8_t> route_enabled;
  std::mutex progress_mutex;
  std::unique_ptr<recv_slot_t> retained_slot;

  std::atomic<uint64_t> send_messages{0};
  std::atomic<uint64_t> send_bytes{0};
  std::atomic<uint64_t> recv_messages{0};
  std::atomic<uint64_t> recv_bytes{0};
  std::atomic<uint64_t> retry_lock{0};
  std::atomic<uint64_t> retry_nomem{0};
  std::atomic<uint64_t> nic_fallbacks{0};
};

context_t alloc_context(runtime_t runtime, bool enable)
{
  context_t context;
  context.p_impl = new context_impl_t;
  context.p_impl->runtime = runtime;
  context.p_impl->enabled = enable;
  context.p_impl->global_rank = bootstrap::get_rank_me();
  context.p_impl->global_size = bootstrap::get_rank_n();
  context.p_impl->global_to_local.assign(context.p_impl->global_size, -1);

  if (context.p_impl->global_size <= 0) return context;

  if (context.p_impl->global_rank == 0) {
    fill_random(context.p_impl->job_nonce.data(),
                context.p_impl->job_nonce.size() * sizeof(uint64_t));
    if (context.p_impl->job_nonce[0] == 0 &&
        context.p_impl->job_nonce[1] == 0) {
      context.p_impl->job_nonce[0] = random_nonzero_u64();
    }
  }
  bootstrap::broadcast(context.p_impl->job_nonce.data(),
                       context.p_impl->job_nonce.data(),
                       context.p_impl->job_nonce.size() * sizeof(uint64_t), 0);

  hostname_record_t send_record;
  if (gethostname(send_record.hostname, sizeof(send_record.hostname)) != 0) {
    std::snprintf(send_record.hostname, sizeof(send_record.hostname),
                  "unknown-%d", context.p_impl->global_rank);
  }
  send_record.hostname[sizeof(send_record.hostname) - 1] = '\0';

  std::vector<hostname_record_t> send_records(context.p_impl->global_size,
                                              send_record);
  std::vector<hostname_record_t> recv_records(context.p_impl->global_size);
  bootstrap::alltoall(send_records.data(), recv_records.data(),
                      sizeof(hostname_record_t));

  const std::string my_host(send_record.hostname);
  for (int rank = 0; rank < context.p_impl->global_size; ++rank) {
    if (my_host == recv_records[rank].hostname) {
      const int local_rank =
          static_cast<int>(context.p_impl->local_to_global.size());
      context.p_impl->global_to_local[rank] = local_rank;
      context.p_impl->local_to_global.push_back(rank);
      if (rank == context.p_impl->global_rank) {
        context.p_impl->local_rank = local_rank;
      }
    }
  }
  context.p_impl->local_size =
      static_cast<int>(context.p_impl->local_to_global.size());
  if (context.p_impl->local_rank < 0) {
    context.p_impl->enabled = false;
  }
  return context;
}

void free_context(context_t* context)
{
  if (context == nullptr || context->p_impl == nullptr) return;
  delete context->p_impl;
  context->p_impl = nullptr;
}

int global_to_local_rank(context_t context, int global_rank)
{
  if (context.is_empty() || global_rank < 0 ||
      global_rank >= context.get_impl()->global_size) {
    return -1;
  }
  return context.get_impl()->global_to_local[global_rank];
}

int local_to_global_rank(context_t context, int local_rank)
{
  if (context.is_empty() || local_rank < 0 ||
      local_rank >= context.get_impl()->local_size) {
    return -1;
  }
  return context.get_impl()->local_to_global[local_rank];
}

device_t alloc_device(context_t context, lci::device_t core_device, bool enable,
                      size_t ring_size, size_t slot_size,
                      size_t max_message_size, size_t max_cas_attempts)
{
  device_t device;
  device.p_impl = new device_impl_t;
  device.p_impl->context = context;
  device.p_impl->core_device = core_device;
  device.p_impl->max_cas_attempts = max_cas_attempts;

  if (!enable || context.is_empty() || !context.get_impl()->enabled ||
      context.get_impl()->global_size <= 0) {
    return device;
  }

  auto* context_impl = context.get_impl();
  const int global_size = context_impl->global_size;
  const int global_rank = context_impl->global_rank;
  device.p_impl->collective_lifecycle = true;

  bool can_create_ring = context_impl->local_size > 1;
  size_t slot_count = 0;
  size_t effective_max = 0;
  if (can_create_ring && (core_device.is_empty() ||
                          core_device.get_impl()->packet_pool.is_empty())) {
    LCI_Warn(
        "Disabling SHM transport because the core device or packet pool is "
        "unavailable\n");
    can_create_ring = false;
  }
  if (can_create_ring) {
    if (slot_size == 0 || ring_size < slot_size ||
        slot_size % LCI_CACHE_LINE != 0 || max_cas_attempts == 0) {
      LCI_Warn("Disabling SHM transport due to invalid slot/ring settings\n");
      can_create_ring = false;
    } else {
      slot_count = ring_size / slot_size;
      const size_t payload_capacity = ring_t::payload_capacity(slot_size);
      if (!is_power_of_two(slot_count) ||
          ring_t::required_size(slot_count, slot_size) == 0 ||
          payload_capacity == 0) {
        LCI_Warn("Disabling SHM transport due to invalid ring geometry\n");
        can_create_ring = false;
      } else {
        const size_t packet_payload =
            core_device.get_impl()->packet_pool.get_impl()->get_payload_size();
        effective_max =
            max_message_size == 0 ? payload_capacity : max_message_size;
        effective_max = std::min(effective_max, payload_capacity);
        effective_max = std::min(effective_max, packet_payload);
        if (effective_max == 0) {
          LCI_Warn(
              "Disabling SHM transport because no packet payload is "
              "available\n");
          can_create_ring = false;
        }
      }
    }
  }

  uint64_t device_uid = 0;
  if (global_rank == 0) {
    device_uid = random_nonzero_u64();
  }
  bootstrap::broadcast(&device_uid, &device_uid, sizeof(device_uid), 0);
  if (device_uid == 0) {
    LCI_Warn("Disabling SHM transport because device UID allocation failed\n");
    can_create_ring = false;
  }

  device.p_impl->device_uid = device_uid;
  device.p_impl->slot_count = slot_count;
  device.p_impl->slot_size = slot_size;
  device.p_impl->max_message_size = effective_max;
  device.p_impl->peer_mappings.resize(global_size);
  device.p_impl->routes.assign(global_size, nullptr);
  device.p_impl->route_enabled.assign(global_size, 0);

  std::string error;
  if (can_create_ring) {
    const std::string owner_name =
        make_object_name(context_impl, device_uid, global_rank);
    device.p_impl->owner = posix_owner_mapping_t::create(
        owner_name, global_rank, device_uid, slot_count, slot_size,
        effective_max, max_cas_attempts, &error);
    if (!device.p_impl->owner) {
      LCI_Warn("Disabling SHM transport; owner ring creation failed: %s\n",
               error.c_str());
    }
  }

  posix_ring_handle_t my_handle;
  if (device.p_impl->owner) my_handle = device.p_impl->owner->handle();
  std::vector<posix_ring_handle_t> send_handles(global_size, my_handle);
  std::vector<posix_ring_handle_t> recv_handles(global_size);
  bootstrap::alltoall(send_handles.data(), recv_handles.data(),
                      sizeof(posix_ring_handle_t));

  if (device.p_impl->owner) {
    const int self = global_rank;
    device.p_impl->routes[self] = &device.p_impl->owner->ring();
    device.p_impl->route_enabled[self] = 1;
  }

  for (int local = 0; can_create_ring && local < context_impl->local_size;
       ++local) {
    const int peer_rank = context_impl->local_to_global[local];
    if (peer_rank == global_rank) continue;
    const auto expected = expected_ring(context_impl, device_uid, peer_rank,
                                        slot_count, slot_size, effective_max);
    error.clear();
    auto peer = posix_peer_mapping_t::attach(recv_handles[peer_rank], expected,
                                             max_cas_attempts, &error);
    if (!peer) {
      LCI_Warn("Disabling SHM route to rank %d: %s\n", peer_rank,
               error.c_str());
      continue;
    }
    device.p_impl->routes[peer_rank] = &peer->ring();
    device.p_impl->route_enabled[peer_rank] = 1;
    device.p_impl->peer_mappings[peer_rank] = std::move(peer);
  }

  for (int rank = 0; rank < context.get_impl()->global_size; ++rank) {
    if (device.p_impl->route_enabled[rank] &&
        test_disables_route(context.get_impl()->global_rank, rank)) {
      LCI_Warn("Test hook disabling SHM route %d -> %d\n",
               context.get_impl()->global_rank, rank);
      device.p_impl->route_enabled[rank] = 0;
      device.p_impl->routes[rank] = nullptr;
    }
  }

  std::vector<uint8_t> local_routes(static_cast<size_t>(global_size), 0);
  for (int rank = 0; rank < global_size; ++rank) {
    local_routes[rank] = device.p_impl->route_enabled[rank];
  }
  std::vector<uint8_t> send_route_matrix(
      static_cast<size_t>(global_size) * global_size, 0);
  for (int dst = 0; dst < global_size; ++dst) {
    std::copy(
        local_routes.begin(), local_routes.end(),
        send_route_matrix.begin() + static_cast<size_t>(dst) * global_size);
  }
  std::vector<uint8_t> recv_route_matrix(
      static_cast<size_t>(global_size) * global_size, 0);
  bootstrap::alltoall(send_route_matrix.data(), recv_route_matrix.data(),
                      static_cast<size_t>(global_size));
  for (int rank = 0; rank < global_size; ++rank) {
    const uint8_t directed = local_routes[rank];
    const uint8_t reciprocal =
        recv_route_matrix[static_cast<size_t>(rank) * global_size +
                          global_rank];
    device.p_impl->route_enabled[rank] = directed && reciprocal;
    if (!device.p_impl->route_enabled[rank]) {
      device.p_impl->routes[rank] = nullptr;
    }
  }

  bootstrap::barrier();
  if (device.p_impl->owner) {
    error.clear();
    if (!device.p_impl->owner->unlink_name(&error)) {
      LCI_Warn("Failed to unlink SHM ring name: %s\n", error.c_str());
    }
  }
  bootstrap::barrier();

  for (int rank = 0; rank < context.get_impl()->global_size; ++rank) {
    if (device.p_impl->route_enabled[rank]) {
      device.p_impl->enabled = true;
      break;
    }
  }
  LCI_Log(LOG_INFO, "shm",
          "SHM device uid=%lu enabled=%d local_rank=%d local_size=%d "
          "slot_count=%lu slot_size=%lu max_message=%lu\n",
          device_uid, static_cast<int>(device.p_impl->enabled),
          context_impl->local_rank, context_impl->local_size, slot_count,
          slot_size, effective_max);
  return device;
}

void free_device(device_t* device)
{
  if (device == nullptr || device->p_impl == nullptr) return;
  auto* impl = device->p_impl;
  const bool collective_lifecycle = impl->collective_lifecycle;
  {
    std::lock_guard<std::mutex> guard(impl->progress_mutex);
    impl->enabled = false;
    if (impl->retained_slot && impl->owner) {
      [[maybe_unused]] bool released =
          impl->owner->ring().release(impl->retained_slot.get());
      LCI_DBG_Assert(released, "Failed to release retained SHM slot\n");
      impl->retained_slot.reset();
    }
  }
  // Device teardown is collective for every rank that participated in SHM
  // setup, even if this rank ended with no agreed route. Once every rank
  // reaches this barrier no peer should initiate new SHM writes through
  // mappings that are about to be unmapped.
  if (collective_lifecycle && !impl->context.is_empty() &&
      impl->context.get_impl()->global_size > 1) {
    bootstrap::barrier();
  }
  impl->peer_mappings.clear();
  impl->routes.clear();
  impl->route_enabled.clear();
  impl->owner.reset();
  delete impl;
  device->p_impl = nullptr;
}

bool is_enabled(device_t device)
{
  return !device.is_empty() && device.get_impl()->enabled;
}

bool can_send(device_t device, int rank, size_t size)
{
  if (!is_enabled(device)) return false;
  auto* impl = device.get_impl();
  return rank >= 0 && rank < impl->context.get_impl()->global_size &&
         size <= impl->max_message_size && impl->route_enabled[rank] &&
         impl->routes[rank] != nullptr;
}

error_t post_send(device_t device, int rank, const void* buffer, size_t size,
                  net_imm_data_t imm_data)
{
  if (!can_send(device, rank, size)) return errorcode_t::fatal;
  auto* impl = device.get_impl();
  error_t error = impl->routes[rank]->post_send(
      impl->context.get_impl()->local_rank, buffer, size, imm_data);
  if (error.is_done()) {
    impl->send_messages.fetch_add(1, std::memory_order_relaxed);
    impl->send_bytes.fetch_add(size, std::memory_order_relaxed);
    LCI_PCOUNTER_ADD(shm_send, 1);
    LCI_PCOUNTER_ADD(shm_send_bytes, size);
  } else if (error.errorcode == errorcode_t::retry_lock) {
    impl->retry_lock.fetch_add(1, std::memory_order_relaxed);
    LCI_PCOUNTER_ADD(shm_retry_lock, 1);
  } else if (error.errorcode == errorcode_t::retry_nomem) {
    impl->retry_nomem.fetch_add(1, std::memory_order_relaxed);
    LCI_PCOUNTER_ADD(shm_retry_nomem, 1);
  }
  return error;
}

bool try_acquire_progress(device_t device)
{
  return is_enabled(device) && device.get_impl()->owner != nullptr &&
         device.get_impl()->progress_mutex.try_lock();
}

void release_progress(device_t device)
{
  LCI_DBG_Assert(!device.is_empty(), "Invalid SHM device\n");
  device.get_impl()->progress_mutex.unlock();
}

std::unique_ptr<recv_slot_t> take_retained_slot(device_t device)
{
  if (device.is_empty()) return nullptr;
  return std::move(device.get_impl()->retained_slot);
}

void retain_slot(device_t device, std::unique_ptr<recv_slot_t> slot)
{
  if (device.is_empty() || !slot) return;
  LCI_DBG_Assert(!device.get_impl()->retained_slot,
                 "Overwriting retained SHM receive slot\n");
  device.get_impl()->retained_slot = std::move(slot);
}

bool poll_comp(device_t device, recv_slot_t* slot)
{
  if (!is_enabled(device) || device.get_impl()->owner == nullptr) return false;
  const bool found = device.get_impl()->owner->ring().poll(slot);
  if (found) {
    device.get_impl()->recv_messages.fetch_add(1, std::memory_order_relaxed);
    device.get_impl()->recv_bytes.fetch_add(slot->size,
                                            std::memory_order_relaxed);
    LCI_PCOUNTER_ADD(shm_recv, 1);
    LCI_PCOUNTER_ADD(shm_recv_bytes, slot->size);
  }
  return found;
}

int recv_source_global_rank(device_t device, const recv_slot_t& slot)
{
  if (device.is_empty()) return -1;
  return local_to_global_rank(device.get_impl()->context,
                              slot.source_local_rank);
}

void release(device_t device, recv_slot_t* slot)
{
  if (device.is_empty() || device.get_impl()->owner == nullptr) return;
  [[maybe_unused]] bool released =
      device.get_impl()->owner->ring().release(slot);
  LCI_DBG_Assert(released, "Failed to release SHM receive slot\n");
}

void note_nic_fallback(device_t device)
{
  if (device.is_empty()) return;
  device.get_impl()->nic_fallbacks.fetch_add(1, std::memory_order_relaxed);
  LCI_PCOUNTER_ADD(shm_nic_fallback, 1);
}

counters_t get_counters(device_t device)
{
  counters_t counters;
  if (device.is_empty()) return counters;
  auto* impl = device.get_impl();
  counters.send_messages = load_counter(impl->send_messages);
  counters.send_bytes = load_counter(impl->send_bytes);
  counters.recv_messages = load_counter(impl->recv_messages);
  counters.recv_bytes = load_counter(impl->recv_bytes);
  counters.retry_lock = load_counter(impl->retry_lock);
  counters.retry_nomem = load_counter(impl->retry_nomem);
  counters.nic_fallbacks = load_counter(impl->nic_fallbacks);
  return counters;
}

}  // namespace shm
}  // namespace lci
