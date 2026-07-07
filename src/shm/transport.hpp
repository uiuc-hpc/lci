// Copyright (c) 2026 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#ifndef LCI_SHM_TRANSPORT_HPP
#define LCI_SHM_TRANSPORT_HPP

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

#include "lci.hpp"
#include "shm/posix_ring.hpp"

namespace lci
{
namespace shm
{
class device_impl_t;

class context_impl_t
{
 public:
  runtime_t runtime;
  bool enabled = false;
  int global_rank = -1;
  int global_size = 0;
  int local_rank = -1;
  int local_size = 0;
  std::vector<int> global_to_local;
  std::vector<int> local_to_global;
  std::array<uint64_t, 2> job_nonce = {{0, 0}};
};

struct context_t {
  context_impl_t* p_impl = nullptr;
  bool is_empty() const { return p_impl == nullptr; }
  context_impl_t* get_impl() const
  {
    if (p_impl == nullptr)
      throw std::runtime_error("shm context p_impl is nullptr!");
    return p_impl;
  }
};

struct device_t {
  device_impl_t* p_impl = nullptr;
  bool is_empty() const { return p_impl == nullptr; }
  device_impl_t* get_impl() const
  {
    if (p_impl == nullptr)
      throw std::runtime_error("shm device p_impl is nullptr!");
    return p_impl;
  }
};

struct counters_t {
  uint64_t send_messages = 0;
  uint64_t send_bytes = 0;
  uint64_t recv_messages = 0;
  uint64_t recv_bytes = 0;
  uint64_t retry_lock = 0;
  uint64_t retry_nomem = 0;
  uint64_t nic_fallbacks = 0;
};

context_t alloc_context(runtime_t runtime, bool enable = true);
void free_context(context_t* context);
int global_to_local_rank(context_t context, int global_rank);
int local_to_global_rank(context_t context, int local_rank);

device_t alloc_device(context_t context, lci::device_t core_device, bool enable,
                      size_t ring_size, size_t slot_size,
                      size_t max_message_size, size_t max_cas_attempts);
void free_device(device_t* device);

bool is_enabled(device_t device);
bool can_send(device_t device, int rank, size_t size);
error_t post_send(device_t device, int rank, const void* buffer, size_t size,
                  net_imm_data_t imm_data);
bool try_acquire_progress(device_t device);
void release_progress(device_t device);
std::unique_ptr<recv_slot_t> take_retained_slot(device_t device);
void retain_slot(device_t device, std::unique_ptr<recv_slot_t> slot);
bool poll_comp(device_t device, recv_slot_t* slot);
int recv_source_global_rank(device_t device, const recv_slot_t& slot);
void release(device_t device, recv_slot_t* slot);
void note_nic_fallback(device_t device);
counters_t get_counters(device_t device);

}  // namespace shm
}  // namespace lci

#endif  // LCI_SHM_TRANSPORT_HPP
