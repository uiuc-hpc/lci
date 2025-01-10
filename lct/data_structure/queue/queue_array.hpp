#ifndef LCI_QUEUE_ARRAY_HPP
#define LCI_QUEUE_ARRAY_HPP

namespace lct
{
template <bool THREAD_SAFE = false>
struct queue_array_t : public queue_base_t {
  explicit queue_array_t(size_t capacity)
      : top(0), bot(0), length(capacity + 1), container(capacity + 1)
  {
    LCT_ASSERT_DIFF_CACHE_LINE(&top, &bot);
    LCT_ASSERT_DIFF_CACHE_LINE(&bot, &length);
    LCT_ASSERT_SAME_CACHE_LINE(&length, &container);
    LCT_ASSERT_DIFF_CACHE_LINE(&container, &lock);
  }
  void push(void* val) override;
  void* pop() override;

 private:
  struct entry_t;
  alignas(LCT_CACHE_LINE) uint_fast64_t top;
  alignas(LCT_CACHE_LINE) uint_fast64_t bot;
  alignas(LCT_CACHE_LINE) uint_fast64_t length;
  std::vector<entry_t> container;
  alignas(LCT_CACHE_LINE) spinlock_t lock;
};

template <bool THREAD_SAFE>
struct alignas(LCT_CACHE_LINE) queue_array_t<THREAD_SAFE>::entry_t {
  entry_t() : data(nullptr) {}
  void* data;
};

template <bool THREAD_SAFE>
void queue_array_t<THREAD_SAFE>::push(void* val)
{
  if constexpr (THREAD_SAFE) lock.lock();
  size_t new_top = (top + 1) % length;
  LCT_DBG_Assert(LCT_log_ctx_default, new_top != length, "the queue is full\n");
  container[top].data = val;
  top = new_top;
  if constexpr (THREAD_SAFE) lock.unlock();
}

template <bool THREAD_SAFE>
void* queue_array_t<THREAD_SAFE>::pop()
{
  void* ret = nullptr;
  if constexpr (THREAD_SAFE)
    if (!lock.try_lock()) return ret;
  if (top != bot) {
    ret = container[bot].data;
    bot = (bot + 1) % length;
  }
  if constexpr (THREAD_SAFE) lock.unlock();
  return ret;
}
}  // namespace lct

#endif  // LCI_QUEUE_ARRAY_HPP
