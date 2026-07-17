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

context_t alloc_context(runtime_t runtime, bool enable = true);
void free_context(context_t* context);

device_t alloc_device(context_t context, lci::device_t core_device, bool enable,
                      size_t ring_size, size_t slot_size);
void free_device(device_t* device);

bool is_enabled(device_t device);
bool can_send(device_t device, int rank, size_t size);
error_t post_send(device_t device, int rank, const void* buffer, size_t size,
                  net_imm_data_t imm_data, bool allow_retry);
size_t recv_available_approx(device_t device);
bool poll_comp(device_t device, recv_slot_t* slot);
void release(device_t device, recv_slot_t* slot);

}  // namespace shm
}  // namespace lci

#endif  // LCI_SHM_TRANSPORT_HPP
