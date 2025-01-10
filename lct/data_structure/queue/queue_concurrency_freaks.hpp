#ifndef LCI_QUEUE_CONCURRENCY_FREAKS_HPP
#define LCI_QUEUE_CONCURRENCY_FREAKS_HPP

#include "third_party/ConcurrencyFreaks/MichaelScottQueue.hpp"
#include "third_party/ConcurrencyFreaks/LCRQueue.hpp"
#include "third_party/ConcurrencyFreaks/array/FAAArrayQueue.hpp"
#include "third_party/ConcurrencyFreaks/array/LazyIndexArrayQueue.hpp"
#include "third_party/lprq/LPRQueue.hpp"

namespace lct
{
template <typename T>
struct queue_concurrency_freaks_t : public queue_base_t {
  queue_concurrency_freaks_t();
  void push(void* val) override;
  void* pop() override;

 private:
  // TODO: Figure out the best metaparameter.
  T queue;
};

template <typename T>
queue_concurrency_freaks_t<T>::queue_concurrency_freaks_t() : queue()
{
}

template <typename T>
void queue_concurrency_freaks_t<T>::push(void* val)
{
  int tid = LCT_get_thread_id();
  LCT_Assert(LCT_log_ctx_default, tid < HazardPointers<T>::HP_MAX_THREADS,
             "Too many threads for this queue.");
  queue.enqueue(val, tid);
}

template <typename T>
void* queue_concurrency_freaks_t<T>::pop()
{
  int tid = LCT_get_thread_id();
  LCT_Assert(LCT_log_ctx_default, tid < HazardPointers<T>::HP_MAX_THREADS,
             "Too many threads for this queue.");
  return queue.dequeue(tid);
}
}  // namespace lct

#endif  // LCI_QUEUE_CONCURRENCY_FREAKS_HPP
