// Copyright (c) 2026 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include "shm/posix_ring.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace lci
{
namespace shm
{
namespace
{
void set_error(std::string* error, const std::string& message)
{
  if (error != nullptr) *error = message;
}

void set_errno_error(std::string* error, const char* operation)
{
  if (error != nullptr) {
    *error = std::string(operation) + ": " + std::strerror(errno);
  }
}

bool valid_object_name(const char* name, size_t capacity, size_t* length)
{
  const void* terminator = std::memchr(name, '\0', capacity);
  if (terminator == nullptr) return false;
  const size_t size = static_cast<const char*>(terminator) - name;
  if (size < 2 || name[0] != '/') return false;
  if (std::memchr(name + 1, '/', size - 1) != nullptr) return false;
  for (size_t i = size + 1; i < capacity; ++i) {
    if (name[i] != '\0') return false;
  }
  if (length != nullptr) *length = size;
  return true;
}

bool valid_expected(const posix_ring_expected_t& expected, std::string* error)
{
  if (expected.owner_global_rank < 0 || expected.device_uid == 0) {
    set_error(error, "invalid expected POSIX ring owner or device UID");
    return false;
  }
  if (expected.name.size() >= posix_ring_name_capacity ||
      expected.name.find('\0') != std::string::npos ||
      !valid_object_name(expected.name.c_str(), expected.name.size() + 1,
                         nullptr)) {
    set_error(error, "invalid expected POSIX shared-memory object name");
    return false;
  }
  if (expected.slot_count > std::numeric_limits<size_t>::max() ||
      expected.slot_size > std::numeric_limits<size_t>::max()) {
    set_error(error, "expected POSIX ring geometry exceeds local size type");
    return false;
  }
  const size_t mapping_size =
      posix_ring_mapping_size(static_cast<size_t>(expected.slot_count),
                              static_cast<size_t>(expected.slot_size));
  if (mapping_size == 0 || expected.mapping_size != mapping_size ||
      expected.max_message_size >
          ring_t::payload_capacity(static_cast<size_t>(expected.slot_size))) {
    set_error(error, "invalid expected POSIX ring geometry");
    return false;
  }
  return true;
}
}  // namespace

size_t posix_ring_mapping_size(size_t slot_count, size_t slot_size)
{
  const size_t required = ring_t::required_size(slot_count, slot_size);
  if (required == 0) return 0;

  const long page_size_long = sysconf(_SC_PAGESIZE);
  if (page_size_long <= 0 || static_cast<unsigned long>(page_size_long) >
                                 std::numeric_limits<size_t>::max()) {
    return 0;
  }
  const size_t page_size = static_cast<size_t>(page_size_long);
  if (required > std::numeric_limits<size_t>::max() - (page_size - 1)) {
    return 0;
  }
  return ((required + page_size - 1) / page_size) * page_size;
}

bool validate_posix_ring_handle(const posix_ring_handle_t& handle,
                                std::string* error)
{
  if (error != nullptr) error->clear();
  if (handle.version != posix_ring_handle_version) {
    set_error(error, "unsupported POSIX ring handle version");
    return false;
  }
  if (handle.owner_global_rank < 0 || handle.device_uid == 0) {
    set_error(error, "invalid POSIX ring owner or device UID");
    return false;
  }
  if (!valid_object_name(handle.name, sizeof(handle.name), nullptr)) {
    set_error(error, "invalid POSIX shared-memory object name");
    return false;
  }
  if (handle.slot_count > std::numeric_limits<size_t>::max() ||
      handle.slot_size > std::numeric_limits<size_t>::max()) {
    set_error(error, "POSIX ring geometry exceeds the local size type");
    return false;
  }
  const size_t mapping_size =
      posix_ring_mapping_size(static_cast<size_t>(handle.slot_count),
                              static_cast<size_t>(handle.slot_size));
  if (mapping_size == 0 || handle.mapping_size != mapping_size ||
      handle.max_message_size >
          ring_t::payload_capacity(static_cast<size_t>(handle.slot_size))) {
    set_error(error, "inconsistent POSIX ring geometry or mapping size");
    return false;
  }
  return true;
}

bool validate_posix_ring_handle(const posix_ring_handle_t& handle,
                                const posix_ring_expected_t& expected,
                                std::string* error)
{
  if (error != nullptr) error->clear();
  if (!valid_expected(expected, error) ||
      !validate_posix_ring_handle(handle, error)) {
    return false;
  }
  if (handle.owner_global_rank != expected.owner_global_rank ||
      handle.device_uid != expected.device_uid ||
      handle.mapping_size != expected.mapping_size ||
      handle.slot_count != expected.slot_count ||
      handle.slot_size != expected.slot_size ||
      handle.max_message_size != expected.max_message_size ||
      expected.name != handle.name) {
    set_error(error,
              "received POSIX ring handle does not match expected identity "
              "and geometry");
    return false;
  }
  return true;
}

posix_owner_mapping_t::posix_owner_mapping_t(void* region, size_t region_size,
                                             const posix_ring_handle_t& handle,
                                             size_t max_cas_attempts)
    : region_(region), region_size_(region_size), handle_(handle)
{
  ring_.reset(
      new ring_t(region_, region_size_, static_cast<size_t>(handle_.slot_count),
                 static_cast<size_t>(handle_.slot_size), max_cas_attempts));
}

std::unique_ptr<posix_owner_mapping_t> posix_owner_mapping_t::create(
    const std::string& name, int owner_global_rank, uint64_t device_uid,
    size_t slot_count, size_t slot_size, size_t max_message_size,
    size_t max_cas_attempts, std::string* error)
{
  if (error != nullptr) error->clear();
  if (name.size() >= posix_ring_name_capacity ||
      name.find('\0') != std::string::npos) {
    set_error(error, "POSIX shared-memory object name is too long");
    return nullptr;
  }
  posix_ring_handle_t handle;
  handle.owner_global_rank = owner_global_rank;
  handle.device_uid = device_uid;
  handle.slot_count = slot_count;
  handle.slot_size = slot_size;
  handle.max_message_size = max_message_size;
  handle.mapping_size = posix_ring_mapping_size(slot_count, slot_size);
  std::memcpy(handle.name, name.c_str(), name.size() + 1);
  if (max_cas_attempts == 0 || !validate_posix_ring_handle(handle, error) ||
      handle.mapping_size >
          static_cast<uint64_t>(std::numeric_limits<off_t>::max())) {
    if (error != nullptr && error->empty()) {
      set_error(error, "invalid POSIX ring allocation arguments");
    }
    return nullptr;
  }

  const int fd = shm_open(handle.name, O_CREAT | O_EXCL | O_RDWR, 0600);
  if (fd < 0) {
    set_errno_error(error, "shm_open(create)");
    return nullptr;
  }
  void* region = MAP_FAILED;
  if (ftruncate(fd, static_cast<off_t>(handle.mapping_size)) != 0) {
    set_errno_error(error, "ftruncate");
  } else {
    region = mmap(nullptr, static_cast<size_t>(handle.mapping_size),
                  PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (region == MAP_FAILED) set_errno_error(error, "mmap(owner)");
  }
  const int close_result = close(fd);
  if (region == MAP_FAILED) {
    shm_unlink(handle.name);
    return nullptr;
  }
  if (close_result != 0) {
    set_errno_error(error, "close(owner fd)");
    munmap(region, static_cast<size_t>(handle.mapping_size));
    shm_unlink(handle.name);
    return nullptr;
  }
  if (!ring_t::initialize(region, static_cast<size_t>(handle.mapping_size),
                          slot_count, slot_size)) {
    set_error(error, "failed to initialize POSIX ring mapping");
    munmap(region, static_cast<size_t>(handle.mapping_size));
    shm_unlink(handle.name);
    return nullptr;
  }

  std::unique_ptr<posix_owner_mapping_t> result;
  try {
    result.reset(new posix_owner_mapping_t(
        region, static_cast<size_t>(handle.mapping_size), handle,
        max_cas_attempts));
  } catch (...) {
    munmap(region, static_cast<size_t>(handle.mapping_size));
    shm_unlink(handle.name);
    throw;
  }
  if (!result->ring().is_valid() || !result->ring().is_consistent_empty()) {
    set_error(error, "new POSIX ring mapping failed consistency validation");
    return nullptr;
  }
  return result;
}

posix_owner_mapping_t::~posix_owner_mapping_t()
{
  ring_.reset();
  if (region_ != nullptr) munmap(region_, region_size_);
  if (name_is_linked_) shm_unlink(handle_.name);
}

bool posix_owner_mapping_t::unlink_name(std::string* error)
{
  if (!name_is_linked_) return true;
  if (shm_unlink(handle_.name) != 0 && errno != ENOENT) {
    set_errno_error(error, "shm_unlink");
    return false;
  }
  name_is_linked_ = false;
  return true;
}

posix_peer_mapping_t::posix_peer_mapping_t(void* region, size_t region_size,
                                           const posix_ring_handle_t& handle,
                                           size_t max_cas_attempts)
    : region_(region), region_size_(region_size), handle_(handle)
{
  ring_.reset(
      new ring_t(region_, region_size_, static_cast<size_t>(handle_.slot_count),
                 static_cast<size_t>(handle_.slot_size), max_cas_attempts));
}

std::unique_ptr<posix_peer_mapping_t> posix_peer_mapping_t::attach(
    const posix_ring_handle_t& handle, const posix_ring_expected_t& expected,
    size_t max_cas_attempts, std::string* error)
{
  if (error != nullptr) error->clear();
  if (max_cas_attempts == 0 ||
      !validate_posix_ring_handle(handle, expected, error)) {
    if (error != nullptr && error->empty()) {
      set_error(error, "invalid POSIX ring attachment arguments");
    }
    return nullptr;
  }
  const int fd = shm_open(handle.name, O_RDWR, 0600);
  if (fd < 0) {
    set_errno_error(error, "shm_open(attach)");
    return nullptr;
  }
  struct stat status = {};
  if (fstat(fd, &status) != 0) {
    set_errno_error(error, "fstat");
    close(fd);
    return nullptr;
  }
  if (status.st_size < 0 ||
      static_cast<uint64_t>(status.st_size) != handle.mapping_size) {
    set_error(error, "POSIX shared-memory object has an unexpected size");
    close(fd);
    return nullptr;
  }
  void* region = mmap(nullptr, static_cast<size_t>(handle.mapping_size),
                      PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (region == MAP_FAILED) {
    set_errno_error(error, "mmap(peer)");
    close(fd);
    return nullptr;
  }
  if (close(fd) != 0) {
    set_errno_error(error, "close(peer fd)");
    munmap(region, static_cast<size_t>(handle.mapping_size));
    return nullptr;
  }

  std::unique_ptr<posix_peer_mapping_t> result;
  try {
    result.reset(new posix_peer_mapping_t(
        region, static_cast<size_t>(handle.mapping_size), handle,
        max_cas_attempts));
  } catch (...) {
    munmap(region, static_cast<size_t>(handle.mapping_size));
    throw;
  }
  if (!result->ring().is_valid() || !result->ring().is_consistent_empty()) {
    set_error(error,
              "attached POSIX ring is not in the initialized empty state");
    return nullptr;
  }
  return result;
}

posix_peer_mapping_t::~posix_peer_mapping_t()
{
  ring_.reset();
  if (region_ != nullptr) munmap(region_, region_size_);
}

}  // namespace shm
}  // namespace lci
