#ifndef LCI_QUEUE_ARRAY_ATOMIC_FAA_HPP
#define LCI_QUEUE_ARRAY_ATOMIC_FAA_HPP

#include <vector>

namespace lct
{
struct queue_array_atomic_faa_t : public queue_base_t {
  explicit queue_array_atomic_faa_t(size_t capacity);
  void push(void* val) override;
  void* pop() override;

 private:
  struct entry_t;
  // point to the next entry that is empty
  alignas(LCT_CACHE_LINE) std::atomic<uint_fast64_t> top;
  // point to the fist entry that is full
  alignas(LCT_CACHE_LINE) std::atomic<uint_fast64_t> bot;
  // queue length
  alignas(LCT_CACHE_LINE) uint_fast64_t length;
  // a pointer to type void*
  std::vector<entry_t> container;
};

struct alignas(LCT_CACHE_LINE) queue_array_atomic_faa_t::entry_t {
  entry_t() : data(nullptr), tag(0) {}
  void* data;
  std::atomic<uint_fast64_t> tag;
};

queue_array_atomic_faa_t::queue_array_atomic_faa_t(size_t capacity)
    : top(0), bot(0), length(capacity + 1), container(capacity + 1)
{
  LCT_ASSERT_DIFF_CACHE_LINE(&top, &bot);
  LCT_ASSERT_DIFF_CACHE_LINE(&bot, &length);
  LCT_ASSERT_SAME_CACHE_LINE(&length, &container);
  static_assert(sizeof(queue_array_atomic_faa_t::entry_t) == LCT_CACHE_LINE,
                "unexpected sizeof(LCM_aqueue_entry_t)");
  // Initialize the tag to be in the empty state
  for (int i = 0; i < container.size(); ++i) {
    container[i].tag.store(i - length + 1);
  }
}

void queue_array_atomic_faa_t::push(void* val)
{
  uint_fast64_t current_top = top.fetch_add(1, std::memory_order_relaxed);
  // make sure the queue is not full
  LCT_DBG_Assert(LCT_log_ctx_default,
                 container[current_top % length].tag.load(
                     std::memory_order_acquire) == current_top - length + 1,
                 "wrote to a nonempty value!\n");
  // write to the slot
  container[current_top % length].data = val;
  // update tag to tell the consumers they can safely read this slot.
  container[current_top % length].tag.store(current_top,
                                            std::memory_order_release);
}

void* queue_array_atomic_faa_t::pop()
{
  uint_fast64_t current_bot = bot.load(std::memory_order_relaxed);
  if (container[current_bot % length].tag.load(std::memory_order_acquire) !=
      current_bot) {
    // the queue is empty
    return nullptr;
  }
  current_bot = bot.fetch_add(1, std::memory_order_relaxed);
  while (container[current_bot % length].tag.load(std::memory_order_acquire) !=
         current_bot) {
    // some thread is ahead of us. We got a cell that is empty.
    uint_fast64_t expected = current_bot + 1;
    bool succeed = bot.compare_exchange_weak(expected, current_bot,
                                             std::memory_order_relaxed,
                                             std::memory_order_relaxed);
    if (succeed) return nullptr;
  }
  // we have successfully reserve an entry
  void* result = container[current_bot % length].data;
#ifdef LCT_DEBUG
  container[current_bot % length].tag.store(current_bot + 1,
                                            std::memory_order_release);
#endif
  return result;
}
}  // namespace lct

#endif  // LCI_QUEUE_ARRAY_ATOMIC_FAA_HPP
