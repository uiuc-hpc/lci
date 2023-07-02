#ifndef LCI_QUEUE_ARRAY_ATOMIC_BASIC_HPP
#define LCI_QUEUE_ARRAY_ATOMIC_BASIC_HPP

#include <vector>

namespace lct
{
struct queue_array_atomic_basic_t : public queue_base_t {
  explicit queue_array_atomic_basic_t(size_t capacity);
  void push(void* val) override;
  void* pop() override;

 private:
  struct entry_t;
  // point to the next entry that is empty
  alignas(LCT_CACHE_LINE) std::atomic<uint_fast64_t> top;
  std::atomic<uint_fast64_t> top2;
  // point to the fist entry that is full
  alignas(LCT_CACHE_LINE) std::atomic<uint_fast64_t> bot;
  std::atomic<uint_fast64_t> bot2;
  // queue length
  alignas(LCT_CACHE_LINE) uint_fast64_t length;
  // a pointer to type void*
  std::vector<entry_t> container;
};

struct alignas(LCT_CACHE_LINE) queue_array_atomic_basic_t::entry_t {
  entry_t() : data(nullptr) {}
  void* data;
};

queue_array_atomic_basic_t::queue_array_atomic_basic_t(size_t capacity)
    : top(0),
      top2(0),
      bot(0),
      bot2(0),
      length(capacity + 1),
      container(capacity + 1)
{
  LCT_ASSERT_SAME_CACHE_LINE(&top, &top2);
  LCT_ASSERT_DIFF_CACHE_LINE(&top2, &bot);
  LCT_ASSERT_SAME_CACHE_LINE(&bot, &bot2);
  LCT_ASSERT_DIFF_CACHE_LINE(&bot2, &length);
  LCT_ASSERT_SAME_CACHE_LINE(&length, &container);
  static_assert(sizeof(queue_array_atomic_basic_t::entry_t) == LCT_CACHE_LINE,
                "unexpected sizeof(LCM_aqueue_entry_t)");
}

void queue_array_atomic_basic_t::push(void* val)
{
  uint_fast64_t current_top = top.fetch_add(1, std::memory_order_relaxed);
  // make sure the queue is not full
  LCT_DBG_Assert(
      LCT_log_ctx_default,
      current_top - bot2.load(std::memory_order_acquire) > length - 2,
      "wrote to a nonempty value!\n");
  // write to the slot
  container[current_top % length].data = val;
  // update top2 to tell the consumers they can safely read this slot.
  while (true) {
    uint_fast64_t expected = current_top;
    bool succeed = top2.compare_exchange_weak(expected, current_top + 1,
                                              std::memory_order_release,
                                              std::memory_order_relaxed);
    if (succeed) {
      // succeed!
      break;
    }
  }
}

void* queue_array_atomic_basic_t::pop()
{
  uint_fast64_t current_bot = bot.load(std::memory_order_relaxed);
  if (top2.load(std::memory_order_acquire) <= current_bot) {
    // the queue is empty
    return nullptr;
  }
  uint_fast64_t expected = current_bot;
  bool succeed = bot.compare_exchange_strong(expected, current_bot + 1,
                                             std::memory_order_relaxed,
                                             std::memory_order_relaxed);
  if (!succeed) {
    // some thread is ahead of us.
    return nullptr;
  }
  // we have successfully reserve an entry
  void* result = container[current_bot % length].data;
#ifdef LCT_DEBUG
  // now that we got the value, we can update bot2 to tell the producers they
  // can safely write to this entry.
  while (true) {
    expected = current_bot;
    succeed = bot2.compare_exchange_weak(expected, current_bot + 1,
                                         std::memory_order_release,
                                         std::memory_order_relaxed);
    if (succeed) {
      // succeed!
      break;
    }
  }
#endif
  return result;
}
}  // namespace lct

#endif  // LCI_QUEUE_ARRAY_ATOMIC_BASIC_HPP
