#include <vector>
#include <atomic>
#include "lcti.hpp"

namespace lct::tlptr
{
std::atomic<uint64_t> next_id;
thread_local std::vector<ptr_t> tl_tlptrs;

tlptr_t::tlptr_t() : id(next_id++), allptrs(128, nullptr) {}

tlptr_t::~tlptr_t() {}

void tlptr_t::set(ptr_t ptr)
{
  if (id >= tl_tlptrs.size()) tl_tlptrs.resize(id * 2 + 1);
  tl_tlptrs[id] = ptr;
  // update allptrs
  lock.lock();
  int thread_id = LCT_get_thread_id();
  if (thread_id >= allptrs.size()) allptrs.resize(thread_id * 2 + 1, nullptr);
  allptrs[thread_id] = ptr;
  lock.unlock();
}

ptr_t tlptr_t::get()
{
  if (LCT_unlikely(id >= tl_tlptrs.size() || tl_tlptrs[id] == nullptr)) {
    // This is the first time this thread accessing this tlptr
    return nullptr;
  }
  return tl_tlptrs[id];
}

std::vector<ptr_t> tlptr_t::get_all()
{
  lock.lock();
  auto ret = allptrs;
  lock.unlock();
  return ret;
}
}  // namespace lct::tlptr
