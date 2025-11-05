// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include "lci_internal.hpp"

#if LCI_USE_REG_CACHE

#include "lci_ucx_api.h"

namespace lci
{
namespace
{
struct UcxEnvGuard {
  UcxEnvGuard();
  ~UcxEnvGuard();

  static size_t ref_count;
};

size_t UcxEnvGuard::ref_count = 0;

UcxEnvGuard::UcxEnvGuard()
{
  if (ref_count++ == 0) {
    LCII_ucs_init();
  }
}

UcxEnvGuard::~UcxEnvGuard()
{
  LCI_Assert(ref_count > 0, "RegCache UCX env refcount underflow");
  if (--ref_count == 0) {
    LCII_ucs_cleanup();
  }
}
}  // namespace

// Internal implementation that holds UCX rcache and ops
struct rcache_entry_generic_t {
  ucs_rcache_region_t super;
  mr_impl_t* mr;
};

class RegCache::impl
{
 public:
  explicit impl(device_impl_t* dev) : ucx_guard_(), rcache_(nullptr), dev_(dev)
  {
    init();
  }
  ~impl() { destroy(); }

  mr_t get(void* address, size_t size)
  {
    mr_t mr{};
    ucs_rcache_region_t* region = nullptr;
    ucs_status_t st = LCII_ucs_rcache_get(
        rcache_, address, size, PROT_READ | PROT_WRITE, nullptr, &region);
    LCI_Assert(st == UCS_OK && region != nullptr, "UCX rcache get failed: %d\n",
               (int)st);
    auto* entry = reinterpret_cast<rcache_entry_generic_t*>(region);
    mr.p_impl = entry->mr;
    mr.p_impl->rcache_region = region;
    return mr;
  }

  void put(mr_impl_t* mr)
  {
    if (!mr || !mr->rcache_region) return;
    LCII_ucs_rcache_region_put(
        rcache_, reinterpret_cast<ucs_rcache_region_t*>(mr->rcache_region));
  }

  static ucs_rcache_ops_t& ops()
  {
    static ucs_rcache_ops_t ops{};
    if (!ops.mem_reg) {
      ops.mem_reg = &mem_reg_cb;
      ops.mem_dereg = &mem_dereg_cb;
      ops.dump_region = &dump_region_cb;
    }
    return ops;
  }

  UcxEnvGuard ucx_guard_;
  ucs_rcache_t* rcache_ = nullptr;
  device_impl_t* dev_ = nullptr;

 private:
  static ucs_status_t mem_reg_cb(void* context, ucs_rcache_t* rcache, void* arg,
                                 ucs_rcache_region_t* rregion, uint16_t flags);
  static void mem_dereg_cb(void* context, ucs_rcache_t* rcache,
                           ucs_rcache_region_t* rregion);
  static void dump_region_cb(void* context, ucs_rcache_t* rcache,
                             ucs_rcache_region_t* rregion, char* buf,
                             size_t max);

  void init()
  {
    ucs_rcache_params_t params{};
    params.region_struct_size = sizeof(rcache_entry_generic_t);
    params.alignment = UCS_RCACHE_MIN_ALIGNMENT;
    params.max_alignment = UCS_RCACHE_MIN_ALIGNMENT;
    params.ucm_events = UCM_EVENT_VM_UNMAPPED | UCM_EVENT_MEM_TYPE_FREE;
    params.ucm_event_priority = 500;  // lower than transports
    params.ops = &ops();
    params.context = dev_;
    params.flags = UCS_RCACHE_FLAG_PURGE_ON_FORK;
    params.max_regions = (unsigned long)-1;
    params.max_size = (size_t)-1;
    params.max_unreleased = (size_t)-1;

    if (LCII_ucs_rcache_create(&params, "lci_rcache_device",
                               LCII_ucs_stats_get_root(), &rcache_) != UCS_OK) {
      rcache_ = nullptr;
    }
  }

  void destroy()
  {
    if (rcache_) {
      LCII_ucs_rcache_destroy(rcache_);
      rcache_ = nullptr;
    }
  }
};

ucs_status_t RegCache::impl::mem_reg_cb(void* context, ucs_rcache_t* /*rcache*/,
                                        void* /*arg*/,
                                        ucs_rcache_region_t* rregion,
                                        uint16_t /*flags*/)
{
  auto* dev = reinterpret_cast<device_impl_t*>(context);
  uintptr_t start = rregion->super.start;
  size_t len = rregion->super.end - rregion->super.start;
  // Call backend implementation directly (no wrapper) to avoid recursion
  mr_t mr = dev->register_memory_impl(reinterpret_cast<void*>(start), len);
  auto* entry = reinterpret_cast<rcache_entry_generic_t*>(rregion);
  entry->mr = mr.p_impl;
  entry->mr->rcache_region = rregion;
  // Ensure required MR fields are set
  entry->mr->device = dev->device;
  entry->mr->address = reinterpret_cast<void*>(start);
  entry->mr->size = len;
  return UCS_OK;
}

void RegCache::impl::mem_dereg_cb(void* context, ucs_rcache_t* /*rcache*/,
                                  ucs_rcache_region_t* rregion)
{
  auto* dev = reinterpret_cast<device_impl_t*>(context);
  auto* entry = reinterpret_cast<rcache_entry_generic_t*>(rregion);
  if (entry->mr) {
    dev->deregister_memory_impl(entry->mr);
    entry->mr = nullptr;
  }
}

void RegCache::impl::dump_region_cb(void* /*context*/, ucs_rcache_t* /*rcache*/,
                                    ucs_rcache_region_t* /*rregion*/, char* buf,
                                    size_t max)
{
  if (max) buf[0] = '\0';
}

RegCache::RegCache(device_impl_t* dev) : p_(new impl(dev)) {}
RegCache::~RegCache()
{
  delete p_;
  p_ = nullptr;
}
mr_t RegCache::get(void* address, size_t size)
{
  return p_->get(address, size);
}
void RegCache::put(mr_impl_t* mr) { p_->put(mr); }

}  // namespace lci

#else  // LCI_USE_REG_CACHE

namespace lci
{
RegCache::RegCache(device_impl_t*)
{
  LCI_Assert(false, "Registration cache is disabled at compile time");
}

RegCache::~RegCache() = default;

mr_t RegCache::get(void*, size_t)
{
  LCI_Assert(false, "Registration cache is disabled at compile time");
  return {};
}

void RegCache::put(mr_impl_t*)
{
  // Nothing to do when the cache is disabled.
}

}  // namespace lci

#endif  // LCI_USE_REG_CACHE
