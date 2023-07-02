#ifndef LCI_QUEUE_BASE_HPP
#define LCI_QUEUE_BASE_HPP

#include <atomic>

namespace lct
{
struct queue_base_t {
  virtual ~queue_base_t() = default;
  virtual void push(void* val) = 0;
  virtual void* pop() = 0;
};

}  // namespace lct

#endif  // LCI_QUEUE_BASE_HPP
