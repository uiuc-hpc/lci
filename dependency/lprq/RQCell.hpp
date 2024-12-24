#pragma once

#include <atomic>

namespace detail
{
template <class, bool padded>
struct CRQCell;

template <class T>
struct CRQCell<T, true> {
  std::atomic<T> val;
  std::atomic<uint64_t> idx;
  uint64_t pad[14];
} __attribute__((aligned(128)));

template <class T>
struct CRQCell<T, false> {
  std::atomic<T> val;
  std::atomic<uint64_t> idx;
} __attribute__((aligned(16)));

template <class, bool>
struct PlainCell;

template <class T>
struct alignas(128) PlainCell<T, true> {
  std::atomic<T> val;
  uint64_t pad[15];
};

template <class T>
struct PlainCell<T, false> {
  std::atomic<T> val;
};

}  // namespace detail
