// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#ifndef LCI_RHANDLER_REGISTRY_H
#define LCI_RHANDLER_REGISTRY_H

namespace lci
{
// minimum rhandler is 1
class rhandler_registry_t
{
 public:
  enum class type_t {
    none,
    comp,
    matching_engine,
  };
  struct entry_t {
    type_t type = type_t::none;
    void* value = nullptr;
    uint64_t metadata = 0;
    entry_t() = default;
    entry_t(type_t type, void* value, uint64_t metadata = 0)
        : type(type), value(value), metadata(metadata)
    {
    }
  };

  rhandler_registry_t() : size(0), entries(32) {}
  ~rhandler_registry_t() = default;

  uint32_t reserve(uint32_t n)
  {
    int idx = size.fetch_add(n);
    return encode_idx(idx);
  }

  void register_rhandler(uint32_t idx, entry_t entry)
  {
    idx = decode_idx(idx);
    LCI_Assert(idx < size, "idx (%u) >= size (%lu)\n", idx, size.load());
    entries.put(idx, entry);
  }

  // this is not thread-safe
  uint32_t register_rhandler(entry_t entry)
  {
    uint32_t idx = size++;
    entries.put(idx, entry);
    return encode_idx(idx);
  }

  // this is not thread-safe
  void deregister_rhandler(uint32_t idx)
  {
    idx = decode_idx(idx);
    LCI_Assert(idx < size, "idx (%u) >= size (%lu)\n", idx, size.load());
    entries.put(idx, entry_t());
  }

  entry_t get(uint32_t idx)
  {
    idx = decode_idx(idx);
    LCI_Assert(idx < size, "idx (%u) >= size (%lu)\n", idx, size.load());
    return entries.get(idx);
  }

 private:
  inline static uint32_t encode_idx(uint32_t idx) { return idx + 1; }
  inline static uint32_t decode_idx(uint32_t idx) { return idx - 1; }

  std::atomic<uint32_t> size;
  mpmc_array_t<entry_t> entries;
};

}  // namespace lci

#endif  // LCI_RHANDLER_REGISTRY_H