// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#ifndef LCI_UTIL_MISC_HPP
#define LCI_UTIL_MISC_HPP

#define LCIU_CONCAT3(a, b, c) LCIU_CONCAT_INNER3(a, b, c)
#define LCIU_CONCAT2(a, b) LCIU_CONCAT_INNER2(a, b)
#define LCIU_CONCAT_INNER3(a, b, c) a##b##c
#define LCIU_CONCAT_INNER2(a, b) a##b
#define LCIU_CACHE_PADDING(size) \
  char LCIU_CONCAT2(padding, __LINE__)[LCI_CACHE_LINE - (size) % LCI_CACHE_LINE]

#define LCI_UNUSED(expr) \
  do {                   \
    (void)(expr);        \
  } while (0)

namespace lci
{
template <typename T>
T get_env_or(const char* env, T default_val)
{
  static_assert(std::is_integral<T>::value, "T must be an integral type");
  const char* s = getenv(env);
  if (s) {
    return atoi(s);
  } else {
    return default_val;
  }
}

template <>
inline const char* get_env_or(const char* env, const char* default_val)
{
  const char* s = getenv(env);
  if (s) {
    return s;
  } else {
    return default_val;
  }
}

static inline size_t get_page_size()
{
  static size_t page_size = sysconf(_SC_PAGESIZE);
  return page_size;
}

static inline void* alloc_memalign(size_t size,
                                   size_t alignment = LCI_CACHE_LINE)
{
  void* p_ptr = nullptr;
  int ret = posix_memalign(&p_ptr, alignment, size);
  LCI_Assert(
      ret == 0, "posix_memalign(%lu, %lu) returned %d (Free memory %lu/%lu)\n",
      alignment, size, ret, sysconf(_SC_AVPHYS_PAGES) * sysconf(_SC_PAGESIZE),
      sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGESIZE));
  return p_ptr;
}

static inline uint32_t set_bits32(uint32_t flag, uint32_t val, int width,
                                  int offset)
{
  LCI_Asserts(width >= 0);
  LCI_Asserts(width <= 32);
  LCI_Asserts(offset >= 0);
  LCI_Asserts(offset <= 32);
  LCI_Asserts(offset + width <= 32);
  const uint32_t val_mask = ((1UL << width) - 1);
  const uint32_t flag_mask = val_mask << offset;
  LCI_Asserts(val <= val_mask);
  flag &= ~flag_mask;                  // clear the bits
  flag |= (val & val_mask) << offset;  // set the bits
  return flag;
}

static inline uint32_t get_bits32(uint32_t flag, int width, int offset)
{
  LCI_Asserts(width >= 0);
  LCI_Asserts(width <= 32);
  LCI_Asserts(offset >= 0);
  LCI_Asserts(offset <= 32);
  LCI_Asserts(offset + width <= 32);
  return (flag >> offset) & ((1UL << width) - 1);
}

static inline uint64_t set_bits64(uint64_t flag, uint64_t val, int width,
                                  int offset)
{
  LCI_Asserts(width >= 0);
  LCI_Asserts(width <= 64);
  LCI_Asserts(offset >= 0);
  LCI_Asserts(offset <= 64);
  LCI_Asserts(offset + width <= 64);
  const uint64_t val_mask = ((1UL << width) - 1);
  const uint64_t flag_mask = val_mask << offset;
  LCI_Asserts(val <= val_mask);
  flag &= ~flag_mask;                  // clear the bits
  flag |= (val & val_mask) << offset;  // set the bits
  return flag;
}

static inline uint64_t get_bits64(uint64_t flag, int width, int offset)
{
  LCI_Asserts(width >= 0);
  LCI_Asserts(width <= 64);
  LCI_Asserts(offset >= 0);
  LCI_Asserts(offset <= 64);
  LCI_Asserts(offset + width <= 64);
  return (flag >> offset) & ((1UL << width) - 1);
}

template <typename T>
struct padded_atomic_t {
  std::atomic<T> val;
  char padding[LCI_CACHE_LINE - sizeof(std::atomic<T>)];
  padded_atomic_t() = default;
  padded_atomic_t(T val_) : val(val_) {}
};

}  // namespace lci

#endif  // LCI_UTIL_MISC_HPP