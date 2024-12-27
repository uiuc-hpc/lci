#pragma once

#include <array>
#include <type_traits>

template <size_t size, size_t cell_size, size_t cache_line_size = 128>
class CacheRemap
{
 private:
  static_assert(cache_line_size % cell_size == 0);
  static_assert(size * cell_size % cache_line_size == 0);

  static constexpr size_t cellsPerCacheLine = cache_line_size / cell_size;
  static constexpr size_t numCacheLines = size * cell_size / cache_line_size;

 public:
  constexpr static bool REMAP = true;

  constexpr inline size_t operator[](size_t i) const noexcept
      __attribute__((always_inline))
  {
    return (i % numCacheLines) * cellsPerCacheLine + i / numCacheLines;
  }
};

class IdentityRemap
{
 public:
  constexpr static bool REMAP = false;

  constexpr inline size_t operator[](size_t i) const noexcept
      __attribute__((always_inline))
  {
    return i;
  }
};

template <bool remap, size_t size, size_t cell_size,
          size_t cache_line_size = 128>
using ConditionalCacheRemap =
    std::conditional_t<remap && (size * cell_size >= cache_line_size),
                       CacheRemap<size, cell_size, cache_line_size>,
                       IdentityRemap>;
