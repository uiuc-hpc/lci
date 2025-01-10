#include <atomic>
#include "lcti.hpp"

std::atomic<int> LCT_nthreads(0);
__thread int LCT_thread_id = -1;

/* thread id */
int LCT_get_thread_id()
{
  if (LCT_unlikely(LCT_thread_id == -1)) {
    LCT_thread_id = LCT_nthreads.fetch_add(1, std::memory_order_relaxed);
  }
  return LCT_thread_id;
}

int LCT_get_nthreads() { return LCT_nthreads.load(std::memory_order_relaxed); }