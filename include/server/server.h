#ifndef SERVER_H_
#define SERVER_H_

#include "config.h"
#include <atomic>
#include <iostream>
#include <memory>
#include <thread>

#include "affinity.h"
#include "profiler.h"

using std::unique_ptr;

struct pinned_pool {
  pinned_pool(void* ptr_) : ptr((uintptr_t)ptr_), last(0) {}
  uintptr_t ptr;
  std::atomic<size_t> last;

  void* allocate()
  {
    ptrdiff_t diff = (ptrdiff_t)(last.fetch_add(1) * sizeof(packet));
    return (void*)(ptr + diff);
  }
};

#ifdef MV_USE_SERVER_OFI
#include "server_ofi.h"
#endif

#ifdef MV_USE_SERVER_RDMAX
#include "server_rdmax.h"
#endif

#endif
