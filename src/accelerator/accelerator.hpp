// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#ifndef LCI_GPU_UTIL_GPU_HPP
#define LCI_GPU_UTIL_GPU_HPP

namespace lci
{
namespace accelerator
{
enum class buffer_type_t {
  HOST,
  DEVICE,
};

struct buffer_attr_t {
  int device;
  buffer_type_t type;
};

void initialize();
void finalize();
buffer_attr_t get_buffer_attr(const void* ptr);

}  // namespace accelerator
}  // namespace lci

#endif  // LCI_GPU_UTIL_GPU_HPP