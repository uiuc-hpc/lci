// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#ifndef LCI_REG_CACHE_HPP
#define LCI_REG_CACHE_HPP

namespace lci
{
class device_impl_t;
class mr_impl_t;
class mr_t;

class RegCache
{
 public:
  explicit RegCache(device_impl_t* dev);
  ~RegCache();

  mr_t get(void* address, size_t size);
  void put(mr_impl_t* mr);
  bool is_valid() const;

  static constexpr bool is_enabled() noexcept { return LCI_USE_REG_CACHE != 0; }

 private:
  struct impl;
  impl* p_ = nullptr;
};

}  // namespace lci

#endif  // LCI_REG_CACHE_HPP
