// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#ifndef LCI_MPMC_ARRAY_HPP
#define LCI_MPMC_ARRAY_HPP

namespace lci
{
// A thread-safe vector supporting efficient read
// We never free old array to ensure read safety
template <typename T>
class mpmc_array_t
{
 public:
  using entry_t = T;
  mpmc_array_t(size_t default_length)
  {
    ptr = new entry_t[default_length];
    length = default_length;
    memset(ptr, 0, length * sizeof(entry_t));
  }

  ~mpmc_array_t()
  {
    for (auto ptr : old_ptrs) {
      delete[] ptr;
    }
    delete[] ptr;
  }

  void resize_nolock(size_t new_length)
  {
    LCI_Assert(new_length > length, "new_length (%lu) <= length (%lu)\n",
               new_length, length.load());
    entry_t* new_ptr = new entry_t[new_length];
    memcpy(new_ptr, ptr, length * sizeof(entry_t));
    for (size_t i = length; i < new_length; i++) {
      new_ptr[i] = entry_t();
    }
    old_ptrs.push_back(ptr);
    ptr = new_ptr;
    length = new_length;
  }

  void put(size_t idx, entry_t val)
  {
    lock.lock();
    if (idx >= length) {
      resize_nolock((idx + 1) * 2);
    }
    ptr[idx] = val;
    lock.unlock();
  }

  entry_t get(size_t idx) const
  {
    if (idx >= length) {
      return entry_t();
    }
    entry_t ret = ptr[idx];
    return ret;
  }

  size_t get_size() { return length; }

 private:
  spinlock_t lock;
  std::atomic<entry_t*> ptr;
  std::atomic<size_t> length;
  std::vector<entry_t*> old_ptrs;
};
}  // namespace lci

#endif  // LCI_MPMC_ARRAY_HPP