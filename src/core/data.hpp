// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#ifndef LCI_CORE_DATA_HPP
#define LCI_CORE_DATA_HPP

namespace lci
{
inline bool register_data(data_t& data, device_t device)
{
  bool did_something = false;
  if (data.is_buffer() && data.buffer.mr.is_empty()) {
    data.buffer.mr =
        register_memory_x(data.buffer.base, data.buffer.size).device(device)();
    did_something = true;
  } else if (data.is_buffers() && data.buffers.buffers[0].mr.is_empty()) {
    for (size_t i = 0; i < data.buffers.count; i++) {
      data.buffers.buffers[i].mr =
          register_memory_x(data.buffers.buffers[i].base,
                            data.buffers.buffers[i].size)
              .device(device)();
    }
    did_something = true;
  }
  return did_something;
}

inline void deregister_data(data_t& data)
{
  if (data.is_buffer()) {
    deregister_memory(&data.buffer.mr);
    data.buffer.mr.set_impl(nullptr);
  } else if (data.is_buffers()) {
    for (size_t i = 0; i < data.buffers.count; i++) {
      deregister_memory(&data.buffers.buffers[i].mr);
      data.buffers.buffers[i].mr.set_impl(nullptr);
    }
  }
}

}  // namespace lci

#endif  // LCI_CORE_DATA_HPP