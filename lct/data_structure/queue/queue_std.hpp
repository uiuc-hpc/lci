#ifndef LCI_QUEUE_STD_HPP
#define LCI_QUEUE_STD_HPP

#include <queue>

namespace lct
{
template <bool THREAD_SAFE = false>
struct queue_std_t : public queue_base_t {
  queue_std_t() = default;
  void push(void* val) override
  {
    if constexpr (THREAD_SAFE) lock.lock();
    queue.push(val);
    if constexpr (THREAD_SAFE) lock.unlock();
  }
  void* pop() override
  {
    if constexpr (THREAD_SAFE)
      if (!lock.try_lock()) return nullptr;
    void* ret = nullptr;
    if (!queue.empty()) {
      ret = queue.front();
      queue.pop();
    }
    if constexpr (THREAD_SAFE) lock.unlock();
    return ret;
  }

 private:
  alignas(LCT_CACHE_LINE) std::queue<void*> queue;
  alignas(LCT_CACHE_LINE) spinlock_t lock;
};
}  // namespace lct

#endif  // LCI_QUEUE_STD_HPP
