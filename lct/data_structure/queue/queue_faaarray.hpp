#ifndef LCI_QUEUE_FAAARRAY_HPP
#define LCI_QUEUE_FAAARRAY_HPP

#include "dependency/ConcurrencyFreaks/array/FAAArrayQueue.hpp"

namespace lct
{
struct queue_faaarray_t : public queue_base_t {
  queue_faaarray_t();
  void push(void* val) override;
  void* pop() override;

 private:
  // TODO: Figure out the best metaparameter.
  FAAArrayQueue<void> queue;
  spinlock_t lock;
};

queue_faaarray_t::queue_faaarray_t() : queue(512 /*Max Threads*/)
{
  fprintf(stderr, "%d: create queue %p\n", LCT_rank, this);
}

void queue_faaarray_t::push(void* val)
{
  lock.lock();
  int tid = LCT_get_thread_id();
  if (tid >= 512)
    throw std::runtime_error("thread id " + std::to_string(tid) +
                             " is too large");
  fprintf(stderr, "%d: queue %p tid %d push %p\n", LCT_rank, this, tid, val);
  std::atomic_thread_fence(std::memory_order_seq_cst);
  queue.enqueue(val, tid);
  lock.unlock();
}

void* queue_faaarray_t::pop()
{
  if (!lock.try_lock()) return nullptr;
  int tid = LCT_get_thread_id();
  if (tid >= 512)
    throw std::runtime_error("thread id " + std::to_string(tid) +
                             " is too large");
  std::atomic_thread_fence(std::memory_order_seq_cst);
  void* ret = queue.dequeue(LCT_get_thread_id());
  if (ret)
    fprintf(stderr, "%d: queue %p tid %d pop %p\n", LCT_rank, this, tid, ret);
  lock.unlock();
  return ret;
}
}  // namespace lct

#endif  // LCI_QUEUE_FAAARRAY_HPP
