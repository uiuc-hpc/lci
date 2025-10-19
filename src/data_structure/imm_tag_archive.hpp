// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#ifndef LCI_DATA_STRUCTURE_IMM_TAG_ARCHIVE_HPP
#define LCI_DATA_STRUCTURE_IMM_TAG_ARCHIVE_HPP

#include <atomic>
#include <cstdint>
#include <limits>

namespace lci
{
class imm_tag_archive_t
{
 public:
  using tag_t = uint32_t;

  explicit imm_tag_archive_t(uint32_t tag_bits = 16)
      : tag_bits_(tag_bits), size_(0), mask_(0), slots_(nullptr), next_slot_(0)
  {
    LCI_Assert(tag_bits_ > 0 && tag_bits_ <= 16,
               "imm_tag_archive_t supports 1-16 bits tags");
    size_ = 1u << tag_bits_;
    mask_ = size_ - 1;
    slots_ = new slot_t[size_];
    const uint64_t empty = empty_value();
    for (uint32_t i = 0; i < size_; ++i) {
      slots_[i].value.store(empty, std::memory_order_relaxed);
    }
  }

  ~imm_tag_archive_t() { delete[] slots_; }

  imm_tag_archive_t(const imm_tag_archive_t&) = delete;
  imm_tag_archive_t& operator=(const imm_tag_archive_t&) = delete;

  tag_t insert(uint64_t value)
  {
    LCI_Assert(value != empty_value(), "Invalid value");
    const uint32_t start =
        next_slot_.fetch_add(1, std::memory_order_relaxed) & mask_;
    const uint64_t empty = empty_value();
    for (uint32_t i = 0; i < size_; ++i) {
      const uint32_t idx = (start + i) & mask_;
      uint64_t expected = empty;
      if (slots_[idx].value.compare_exchange_strong(
              expected, value, std::memory_order_relaxed,
              std::memory_order_relaxed)) {
        return idx;
      }
    }
    LCI_Assert(false, "Immediate data archive exhausted");
    return 0;
  }

  uint64_t remove(tag_t tag)
  {
    LCI_Assert(tag < size_, "Tag out of range");
    const uint64_t empty = empty_value();
    const uint64_t value =
        slots_[tag].value.exchange(empty, std::memory_order_relaxed);
    LCI_Assert(value != empty, "Removing non-existent entry");
    return value;
  }

  uint64_t peek(tag_t tag) const
  {
    LCI_Assert(tag < size_, "Tag out of range");
    return slots_[tag].value.load(std::memory_order_relaxed);
  }

  void clear()
  {
    const uint64_t empty = empty_value();
    for (uint32_t i = 0; i < size_; ++i) {
      slots_[i].value.store(empty, std::memory_order_relaxed);
    }
    next_slot_.store(0, std::memory_order_relaxed);
  }

  uint32_t tag_bits() const { return tag_bits_; }
  uint32_t capacity() const { return size_; }

 private:
  struct alignas(LCI_CACHE_LINE) slot_t {
    std::atomic<uint64_t> value;
  };

  static constexpr uint64_t empty_value()
  {
    return std::numeric_limits<uint64_t>::max();
  }

  uint32_t tag_bits_;
  uint32_t size_;
  uint32_t mask_;
  slot_t* slots_;
  std::atomic<uint32_t> next_slot_;
};
}  // namespace lci

#endif  // LCI_DATA_STRUCTURE_IMM_TAG_ARCHIVE_HPP
