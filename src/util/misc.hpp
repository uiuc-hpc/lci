#ifndef LCIXX_UTIL_MISC_HPP
#define LCIXX_UTIL_MISC_HPP

namespace lcixx
{
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

}  // namespace lcixx

#endif  // LCIXX_UTIL_MISC_HPP