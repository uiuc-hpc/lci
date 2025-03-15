// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#ifndef LCI_COMP_SYNC_HPP
#define LCI_COMP_SYNC_HPP

namespace lci
{
class sync_t : public comp_impl_t
{
 public:
  sync_t(comp_attr_t attr_, int threshold_)
      : comp_impl_t(attr_),
        m_top(0),
        m_top2(0),
        m_tail(0),
        threshold(threshold_),
        statuses(threshold_)
  {
    attr.comp_type = attr_comp_type_t::sync;
    attr.sync_threshold = threshold;
  }

  ~sync_t() = default;

  void signal(status_t status) override
  {
    uint64_t tail = 0;
    uint64_t pos = 0;
    if (threshold > 1) {
      pos = m_top.fetch_add(1, std::memory_order_relaxed);
      tail = m_tail.load(std::memory_order_acquire);
    }
    LCI_Assert(pos < tail + threshold, "Receive more signals than expected\n");
    statuses[pos - tail] = std::move(status);
    m_top2.fetch_add(1, std::memory_order_release);
    LCI_PCOUNTER_ADD(comp_produce, 1);
  }

  bool test(status_t* p_out)
  {
    uint64_t top2 = m_top2.load(std::memory_order_acquire);
    uint64_t tail = m_tail.load(std::memory_order_acquire);
    if (top2 != tail + threshold) {
      return false;
    } else {
      uint64_t expected = tail;
      bool succeed = m_tail.compare_exchange_weak(
          expected, top2, std::memory_order_release, std::memory_order_relaxed);
      if (succeed) {
        if (p_out) {
          for (int i = 0; i < threshold; ++i) {
            p_out[i] = std::move(statuses[i]);
          }
        }
        LCI_PCOUNTER_ADD(comp_consume, threshold);
        return true;
      } else {
        return false;
      }
    }
  }

  void wait(status_t* p_out)
  {
    bool succeed;
    do {
      succeed = test(p_out);
    } while (!succeed);
  }

 private:
  std::atomic<uint64_t> m_top;
  std::atomic<uint64_t> m_top2;
  LCIU_CACHE_PADDING(2 * sizeof(std::atomic<uint64_t>));
  std::atomic<uint64_t> m_tail;
  LCIU_CACHE_PADDING(sizeof(std::atomic<uint64_t>));
  int threshold;
  std::vector<status_t> statuses;
};

inline bool sync_test_x::call_impl(comp_t comp, status_t* p_out,
                                   runtime_t) const
{
  sync_t* p_sync = static_cast<sync_t*>(comp.p_impl);
  return p_sync->test(p_out);
}

inline void sync_wait_x::call_impl(comp_t comp, status_t* p_out,
                                   runtime_t) const
{
  sync_t* p_sync = static_cast<sync_t*>(comp.p_impl);
  p_sync->wait(p_out);
}

}  // namespace lci

#endif  // LCI_COMP_SYNC_HPP