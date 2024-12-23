#ifndef LCI_QUEUE_LPRQ_HPP
#define LCI_QUEUE_LPRQ_HPP

#include "external/LPRQueue.hpp"

namespace lct
{
struct queue_lprq_t : public queue_base_t {
  queue_lprq_t();
  void push(void* val) override;
  void* pop() override;

 private:
  // TODO: Figure out the best metaparameter.
  LPRQueue<void> queue;
};

queue_lprq_t::queue_lprq_t() : queue(512 /*Max Threads*/) {}

void queue_lprq_t::push(void* val) { queue.enqueue(val, LCT_get_thread_id()); }

void* queue_lprq_t::pop() { return queue.dequeue(LCT_get_thread_id()); }
}  // namespace lct

#endif  // LCI_QUEUE_LPRQ_HPP
