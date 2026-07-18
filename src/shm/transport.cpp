// Copyright (c) 2026 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include "lci_internal.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <unistd.h>

namespace lci
{
namespace shm
{
namespace
{
constexpr size_t pmi_max_binary_bytes = (LCT_PMI_STRING_LIMIT - 1) / 2;
constexpr size_t hostname_capacity = 120;

struct hostname_record_t {
  uint8_t valid = 0;
  char hostname[hostname_capacity] = {};
};

static_assert(sizeof(hostname_record_t) <= pmi_max_binary_bytes,
              "hostname record must fit existing PMI encoding");
static_assert(sizeof(posix_ring_handle_t) <= pmi_max_binary_bytes,
              "POSIX SHM handle must fit existing PMI encoding");

size_t round_down_to_power_of_two(size_t value)
{
  size_t rounded = 1;
  while (rounded <= value / 2) {
    rounded *= 2;
  }
  return value == 0 ? 0 : rounded;
}

uint8_t hex_value(char value)
{
  if (value >= '0' && value <= '9') return static_cast<uint8_t>(value - '0');
  if (value >= 'a' && value <= 'f')
    return static_cast<uint8_t>(value - 'a' + 10);
  LCI_Assert(false, "Invalid SHM PMI hex character\n");
  return 0;
}

void encode_pmi_value(const void* input, size_t size, char* output)
{
  static constexpr char hex[] = "0123456789abcdef";
  LCI_Assert(size <= pmi_max_binary_bytes,
             "SHM PMI value of %lu bytes exceeds encoding limit\n", size);
  const auto* bytes = static_cast<const uint8_t*>(input);
  for (size_t i = 0; i < size; ++i) {
    output[2 * i] = hex[bytes[i] >> 4];
    output[2 * i + 1] = hex[bytes[i] & 0xf];
  }
  output[2 * size] = '\0';
}

void decode_pmi_value(const char* input, size_t size, void* output)
{
  LCI_Assert(std::strlen(input) == 2 * size,
             "Invalid SHM PMI encoded value length\n");
  auto* bytes = static_cast<uint8_t*>(output);
  for (size_t i = 0; i < size; ++i) {
    bytes[i] = static_cast<uint8_t>((hex_value(input[2 * i]) << 4) |
                                    hex_value(input[2 * i + 1]));
  }
}

int next_pmi_round()
{
  static std::atomic<int> round{0};
  return round.fetch_add(1, std::memory_order_relaxed);
}

template <typename T>
void pmi_allgather(const T& send_value, std::vector<T>* recv_values)
{
  static_assert(sizeof(T) <= pmi_max_binary_bytes,
                "SHM PMI value exceeds encoding limit");
  const int rank = bootstrap::get_rank_me();
  const int nranks = bootstrap::get_rank_n();
  const int round = next_pmi_round();
  char key[LCT_PMI_STRING_LIMIT] = {};
  char value[LCT_PMI_STRING_LIMIT] = {};
  std::snprintf(key, sizeof(key), "LCI_SHM_%d_%d", round, rank);
  encode_pmi_value(&send_value, sizeof(send_value), value);
  LCT_pmi_publish(key, value);
  LCT_pmi_barrier();

  recv_values->resize(static_cast<size_t>(nranks));
  for (int peer = 0; peer < nranks; ++peer) {
    std::memset(key, 0, sizeof(key));
    std::memset(value, 0, sizeof(value));
    std::snprintf(key, sizeof(key), "LCI_SHM_%d_%d", round, peer);
    LCT_pmi_getname(peer, key, value);
    decode_pmi_value(value, sizeof(T), &(*recv_values)[peer]);
  }
}

template <typename T>
T pmi_broadcast(T value, int root)
{
  std::vector<T> values;
  pmi_allgather(value, &values);
  return values[static_cast<size_t>(root)];
}

void pmi_barrier() { LCT_pmi_barrier(); }

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

std::string make_object_name(context_impl_t* context, uint64_t device_uid,
                             int owner_global_rank)
{
  return make_posix_ring_name(context->job_nonce, device_uid,
                              owner_global_rank);
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
  std::unique_ptr<posix_owner_mapping_t> owner;
  std::vector<std::unique_ptr<posix_peer_mapping_t>> peer_mappings;
  std::vector<ring_t*> routes;
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
  context.p_impl->job_nonce = pmi_broadcast(context.p_impl->job_nonce, 0);

  hostname_record_t send_record;
  if (gethostname(send_record.hostname, sizeof(send_record.hostname)) == 0 &&
      std::memchr(send_record.hostname, '\0', sizeof(send_record.hostname)) !=
          nullptr) {
    send_record.valid = 1;
  }
  send_record.hostname[sizeof(send_record.hostname) - 1] = '\0';

  std::vector<hostname_record_t> recv_records;
  pmi_allgather(send_record, &recv_records);
  const bool locality_ok = std::all_of(
      recv_records.begin(), recv_records.end(),
      [](const hostname_record_t& record) { return record.valid != 0; });
  if (!locality_ok) {
    delete context.p_impl;
    context.p_impl = nullptr;
    throw std::runtime_error(
        "SHM locality discovery failed: gethostname did not succeed on all "
        "ranks");
  }

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
    delete context.p_impl;
    context.p_impl = nullptr;
    throw std::runtime_error(
        "SHM locality discovery failed to identify the local rank");
  }
  return context;
}

void free_context(context_t* context)
{
  if (context == nullptr || context->p_impl == nullptr) return;
  delete context->p_impl;
  context->p_impl = nullptr;
}

device_t alloc_device(context_t context, lci::device_t core_device, bool enable,
                      size_t ring_size, size_t slot_size,
                      size_t producer_cas_attempts,
                      size_t consumer_cas_attempts)
{
  device_t device;
  device.p_impl = new device_impl_t;
  device.p_impl->context = context;
  device.p_impl->core_device = core_device;

  if (!enable || context.is_empty() || !context.get_impl()->enabled ||
      context.get_impl()->global_size <= 0) {
    return device;
  }

  auto* context_impl = context.get_impl();
  const int global_size = context_impl->global_size;
  const int global_rank = context_impl->global_rank;
  device.p_impl->collective_lifecycle = true;

  const bool has_local_peers = context_impl->local_size > 1;
  size_t slot_count = 0;
  size_t effective_max = 0;
  bool local_init_ok = true;
  std::string error;
  if (has_local_peers) {
    if (core_device.is_empty() ||
        core_device.get_impl()->packet_pool.is_empty()) {
      error = "core device packet pool is unavailable";
      local_init_ok = false;
    } else if (slot_size == 0 || ring_size < slot_size ||
               producer_cas_attempts == 0 || consumer_cas_attempts == 0 ||
               slot_size % LCI_CACHE_LINE != 0) {
      error = "invalid slot/ring settings";
      local_init_ok = false;
    } else {
      slot_count = round_down_to_power_of_two(ring_size / slot_size);
      const size_t payload_capacity = ring_t::payload_capacity(slot_size);
      if (slot_count == 0 ||
          ring_t::required_size(slot_count, slot_size) == 0 ||
          payload_capacity == 0) {
        error = "invalid ring geometry";
        local_init_ok = false;
      } else {
        const size_t packet_payload =
            core_device.get_impl()->packet_pool.get_impl()->get_payload_size();
        effective_max = std::min(payload_capacity, packet_payload);
        if (effective_max == 0) {
          error = "packet payload capacity is zero";
          local_init_ok = false;
        }
      }
    }
  }

  uint64_t device_uid = 0;
  if (global_rank == 0) {
    device_uid = random_nonzero_u64();
  }
  device_uid = pmi_broadcast(device_uid, 0);
  if (device_uid == 0) {
    error = "device UID allocation failed";
    local_init_ok = false;
  }

  device.p_impl->device_uid = device_uid;
  device.p_impl->slot_count = slot_count;
  device.p_impl->slot_size = slot_size;
  device.p_impl->max_message_size = effective_max;
  device.p_impl->peer_mappings.resize(global_size);
  device.p_impl->routes.assign(global_size, nullptr);

  if (has_local_peers && local_init_ok) {
    const std::string owner_name =
        make_object_name(context_impl, device_uid, global_rank);
    device.p_impl->owner = posix_owner_mapping_t::create(
        owner_name, global_rank, device_uid, slot_count, slot_size,
        effective_max, producer_cas_attempts, consumer_cas_attempts, &error);
    if (!device.p_impl->owner) {
      local_init_ok = false;
    }
  }

  posix_ring_handle_t my_handle;
  if (device.p_impl->owner) my_handle = device.p_impl->owner->handle();
  std::vector<posix_ring_handle_t> recv_handles;
  pmi_allgather(my_handle, &recv_handles);

  if (device.p_impl->owner) {
    const int self = global_rank;
    device.p_impl->routes[self] = &device.p_impl->owner->ring();
  }

  for (int local = 0; local_init_ok && local < context_impl->local_size;
       ++local) {
    const int peer_rank = context_impl->local_to_global[local];
    if (peer_rank == global_rank) continue;
    const auto expected = expected_ring(context_impl, device_uid, peer_rank,
                                        slot_count, slot_size, effective_max);
    error.clear();
    auto peer = posix_peer_mapping_t::attach(recv_handles[peer_rank], expected,
                                             producer_cas_attempts,
                                             consumer_cas_attempts, &error);
    if (!peer) {
      local_init_ok = false;
      break;
    }
    device.p_impl->routes[peer_rank] = &peer->ring();
    device.p_impl->peer_mappings[peer_rank] = std::move(peer);
  }

  const uint8_t local_status = local_init_ok ? 1 : 0;
  std::vector<uint8_t> recv_status;
  pmi_allgather(local_status, &recv_status);
  const bool global_init_ok =
      std::all_of(recv_status.begin(), recv_status.end(),
                  [](uint8_t status) { return status != 0; });
  if (!global_init_ok) {
    device.p_impl->peer_mappings.clear();
    device.p_impl->routes.clear();
    device.p_impl->owner.reset();
    delete device.p_impl;
    device.p_impl = nullptr;
    throw std::runtime_error("SHM device initialization failed" +
                             (error.empty() ? std::string() : ": " + error));
  }

  pmi_barrier();
  if (device.p_impl->owner) {
    error.clear();
    if (!device.p_impl->owner->unlink_name(&error)) {
      LCI_Warn("Failed to unlink SHM ring name: %s\n", error.c_str());
    }
  }
  pmi_barrier();

  device.p_impl->enabled = has_local_peers;
  LCI_Log(LOG_INFO, "shm",
          "SHM device uid=%lu enabled=%d local_rank=%d local_size=%d "
          "requested_ring_size=%lu effective_ring_size=%lu slot_count=%lu "
          "slot_size=%lu producer_cas_attempts=%lu "
          "consumer_cas_attempts=%lu max_message=%lu\n",
          device_uid, static_cast<int>(device.p_impl->enabled),
          context_impl->local_rank, context_impl->local_size, ring_size,
          slot_count * slot_size, slot_count, slot_size, producer_cas_attempts,
          consumer_cas_attempts, effective_max);
  return device;
}

void free_device(device_t* device)
{
  if (device == nullptr || device->p_impl == nullptr) return;
  auto* impl = device->p_impl;
  const bool collective_lifecycle = impl->collective_lifecycle;
  impl->enabled = false;
  if (collective_lifecycle && !impl->context.is_empty() &&
      impl->context.get_impl()->global_size > 1) {
    pmi_barrier();
  }
  impl->peer_mappings.clear();
  impl->routes.clear();
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
         size <= impl->max_message_size && impl->routes[rank] != nullptr;
}

error_t post_send(device_t device, int rank, const void* buffer, size_t size,
                  net_imm_data_t imm_data, bool allow_retry)
{
  if (!can_send(device, rank, size)) return errorcode_t::fatal;
  auto* impl = device.get_impl();
  error_t error;
  do {
    error = impl->routes[rank]->post_send(impl->context.get_impl()->global_rank,
                                          buffer, size, imm_data);
  } while (!allow_retry && error.errorcode == errorcode_t::retry_lock);
  if (error.is_done()) {
    LCI_PCOUNTER_ADD(shm_send, 1);
    LCI_PCOUNTER_ADD(shm_send_bytes, size);
  } else if (error.errorcode == errorcode_t::retry_nomem) {
    LCI_PCOUNTER_ADD(shm_ring_full, 1);
  }
  return error;
}

size_t recv_available_approx(device_t device)
{
  if (!is_enabled(device) || device.get_impl()->owner == nullptr) return 0;
  return device.get_impl()->owner->ring().recv_available_approx();
}

bool poll_comp(device_t device, recv_slot_t* slot)
{
  if (!is_enabled(device) || device.get_impl()->owner == nullptr) return false;
  const bool found = device.get_impl()->owner->ring().poll(slot);
  if (found) {
    LCI_PCOUNTER_ADD(shm_recv, 1);
    LCI_PCOUNTER_ADD(shm_recv_bytes, slot->size);
  }
  return found;
}

void release(device_t device, recv_slot_t* slot)
{
  if (device.is_empty() || device.get_impl()->owner == nullptr) return;
  [[maybe_unused]] bool released =
      device.get_impl()->owner->ring().release(slot);
  LCI_DBG_Assert(released, "Failed to release SHM receive slot\n");
}

}  // namespace shm
}  // namespace lci
