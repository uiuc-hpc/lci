// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#ifndef LCI_COMP_COUNTER_HPP
#define LCI_COMP_COUNTER_HPP

namespace lci
{
class counter_t : public comp_impl_t
{
 public:
  counter_t()
      : comp_impl_t(),
        count(0)
  {}

  ~counter_t() = default;

  void signal(status_t) override
  {
    count.fetch_add(1, std::memory_order_relaxed);
    LCI_PCOUNTER_ADD(comp_produce, 1);
  }

  void set(int64_t value) { count.store(value, std::memory_order_release); }

  int64_t get() const { return count.load(std::memory_order_acquire); }

 private:
  std::atomic<int64_t> count;
  LCIU_CACHE_PADDING(sizeof(std::atomic<int64_t>));
};

inline int64_t counter_get_x::call_impl(comp_t comp, runtime_t) const
{
  const counter_t* counter = static_cast<const counter_t*>(comp.p_impl);
  return counter->get();
}

inline void counter_set_x::call_impl(comp_t comp, int64_t value,
                                 runtime_t) const
{
  counter_t* counter = static_cast<counter_t*>(comp.p_impl);
  counter->set(value);
}

}  // namespace lci

#endif  // LCI_COMP_COUNTER_HPP