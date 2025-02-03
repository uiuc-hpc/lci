#ifndef LCIXX_UTIL_MISC_HPP
#define LCIXX_UTIL_MISC_HPP

namespace lcixx
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

static inline void* alloc_memalign(size_t alignment, size_t size)
{
  void* p_ptr;
  int ret = posix_memalign(&p_ptr, alignment, size);
  LCIXX_Assert(
      ret == 0, "posix_memalign(%lu, %lu) returned %d (Free memory %lu/%lu)\n",
      alignment, size, ret, sysconf(_SC_AVPHYS_PAGES) * sysconf(_SC_PAGESIZE),
      sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGESIZE));
  return p_ptr;
}

static inline uint32_t set_bits32(uint32_t flag, uint32_t val, int width,
                                  int offset)
{
  LCIXX_Asserts(width >= 0);
  LCIXX_Asserts(width <= 32);
  LCIXX_Asserts(offset >= 0);
  LCIXX_Asserts(offset <= 32);
  LCIXX_Asserts(offset + width <= 32);
  const uint32_t val_mask = ((1UL << width) - 1);
  const uint32_t flag_mask = val_mask << offset;
  LCIXX_Asserts(val <= val_mask);
  flag &= ~flag_mask;                  // clear the bits
  flag |= (val & val_mask) << offset;  // set the bits
  return flag;
}

static inline uint32_t get_bits32(uint32_t flag, int width, int offset)
{
  LCIXX_Asserts(width >= 0);
  LCIXX_Asserts(width <= 32);
  LCIXX_Asserts(offset >= 0);
  LCIXX_Asserts(offset <= 32);
  LCIXX_Asserts(offset + width <= 32);
  return (flag >> offset) & ((1UL << width) - 1);
}

static inline uint64_t set_bits64(uint64_t flag, uint64_t val, int width,
                                  int offset)
{
  LCIXX_Asserts(width >= 0);
  LCIXX_Asserts(width <= 64);
  LCIXX_Asserts(offset >= 0);
  LCIXX_Asserts(offset <= 64);
  LCIXX_Asserts(offset + width <= 64);
  const uint64_t val_mask = ((1UL << width) - 1);
  const uint64_t flag_mask = val_mask << offset;
  LCIXX_Asserts(val <= val_mask);
  flag &= ~flag_mask;                  // clear the bits
  flag |= (val & val_mask) << offset;  // set the bits
  return flag;
}

static inline uint64_t get_bits64(uint64_t flag, int width, int offset)
{
  LCIXX_Asserts(width >= 0);
  LCIXX_Asserts(width <= 64);
  LCIXX_Asserts(offset >= 0);
  LCIXX_Asserts(offset <= 64);
  LCIXX_Asserts(offset + width <= 64);
  return (flag >> offset) & ((1UL << width) - 1);
}

}  // namespace lcixx

#endif  // LCIXX_UTIL_MISC_HPP