// Copyright (c) 2026 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#ifndef LCI_SHM_POSIX_RING_HPP
#define LCI_SHM_POSIX_RING_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "shm/ring.hpp"

namespace lci
{
namespace shm
{
constexpr uint32_t posix_ring_handle_version = 1;
constexpr size_t posix_ring_name_capacity = 128;
// Darwin's POSIX shm namespace is substantially shorter than the bootstrap
// handle storage. Keep generated names within this portable string length
// (including the leading slash, excluding the trailing NUL).
constexpr size_t posix_ring_portable_name_max = 30;

// Fixed-size bootstrap data. It contains no process-local pointer or file
// descriptor; peers validate every field before opening the object.
struct posix_ring_handle_t {
  uint32_t version = posix_ring_handle_version;
  int32_t owner_global_rank = -1;
  uint64_t device_uid = 0;
  uint64_t mapping_size = 0;
  uint64_t slot_count = 0;
  uint64_t slot_size = 0;
  uint64_t max_message_size = 0;
  char name[posix_ring_name_capacity] = {};
};

// Locally authoritative attachment metadata, derived from collective device
// configuration and the expected peer identity/name rather than from the
// received handle. attach() compares every field before opening the object.
struct posix_ring_expected_t {
  int32_t owner_global_rank = -1;
  uint64_t device_uid = 0;
  uint64_t mapping_size = 0;
  uint64_t slot_count = 0;
  uint64_t slot_size = 0;
  uint64_t max_message_size = 0;
  std::string name;
};

bool validate_posix_ring_handle(const posix_ring_handle_t& handle,
                                std::string* error = nullptr);
bool validate_posix_ring_handle(const posix_ring_handle_t& handle,
                                const posix_ring_expected_t& expected,
                                std::string* error = nullptr);
// POSIX object size used for ftruncate()/mmap()/fstat(). This can be larger
// than ring_t::required_size() because some kernels report shm object sizes at
// page granularity.
size_t posix_ring_mapping_size(size_t slot_count, size_t slot_size);
std::string make_posix_ring_name(const std::array<uint64_t, 2>& job_nonce,
                                 uint64_t device_uid, int owner_global_rank);

// Owns the receiver's inbound mapping and its still-linked POSIX object name.
// Destruction unlinks the name if unlink_name() was not already called.
class posix_owner_mapping_t
{
 public:
  static std::unique_ptr<posix_owner_mapping_t> create(
      const std::string& name, int owner_global_rank, uint64_t device_uid,
      size_t slot_count, size_t slot_size, size_t max_message_size,
      size_t max_cas_attempts = 1, std::string* error = nullptr);

  ~posix_owner_mapping_t();
  posix_owner_mapping_t(const posix_owner_mapping_t&) = delete;
  posix_owner_mapping_t& operator=(const posix_owner_mapping_t&) = delete;
  posix_owner_mapping_t(posix_owner_mapping_t&&) = delete;
  posix_owner_mapping_t& operator=(posix_owner_mapping_t&&) = delete;

  const posix_ring_handle_t& handle() const { return handle_; }
  ring_t& ring() { return *ring_; }
  bool unlink_name(std::string* error = nullptr);

 private:
  posix_owner_mapping_t(void* region, size_t region_size,
                        const posix_ring_handle_t& handle,
                        size_t max_cas_attempts);

  void* region_ = nullptr;
  size_t region_size_ = 0;
  posix_ring_handle_t handle_;
  std::unique_ptr<ring_t> ring_;
  bool name_is_linked_ = true;
};

// Owns only a peer mapping. The receiver remains the owner of the object and
// is solely responsible for unlinking its name.
class posix_peer_mapping_t
{
 public:
  static std::unique_ptr<posix_peer_mapping_t> attach(
      const posix_ring_handle_t& handle, const posix_ring_expected_t& expected,
      size_t max_cas_attempts = 1, std::string* error = nullptr);

  ~posix_peer_mapping_t();
  posix_peer_mapping_t(const posix_peer_mapping_t&) = delete;
  posix_peer_mapping_t& operator=(const posix_peer_mapping_t&) = delete;
  posix_peer_mapping_t(posix_peer_mapping_t&&) = delete;
  posix_peer_mapping_t& operator=(posix_peer_mapping_t&&) = delete;

  const posix_ring_handle_t& handle() const { return handle_; }
  ring_t& ring() { return *ring_; }

 private:
  posix_peer_mapping_t(void* region, size_t region_size,
                       const posix_ring_handle_t& handle,
                       size_t max_cas_attempts);

  void* region_ = nullptr;
  size_t region_size_ = 0;
  posix_ring_handle_t handle_;
  std::unique_ptr<ring_t> ring_;
};

}  // namespace shm
}  // namespace lci

#endif  // LCI_SHM_POSIX_RING_HPP
