#pragma once

#include <atomic>
#include "LinkedRingQueue.hpp"
#include "RQCell.hpp"
#include "CacheRemap.hpp"

template <typename T, bool padded_cells, size_t ring_size, bool cache_remap>
class PRQueue
    : public QueueSegmentBase<T,
                              PRQueue<T, padded_cells, ring_size, cache_remap>>
{
 private:
  using Base =
      QueueSegmentBase<T, PRQueue<T, padded_cells, ring_size, cache_remap>>;
  using Cell = detail::CRQCell<void*, padded_cells>;

  Cell array[ring_size];

  [[no_unique_address]] ConditionalCacheRemap<cache_remap, ring_size,
                                              sizeof(Cell)>
      remap{};

  inline uint64_t nodeIndex(uint64_t i) const { return (i & ~(1ull << 63)); }

  inline uint64_t setUnsafe(uint64_t i) const { return (i | (1ull << 63)); }

  inline uint64_t nodeUnsafe(uint64_t i) const { return (i & (1ull << 63)); }

  inline bool isBottom(void* const value) const
  {
    return (reinterpret_cast<uintptr_t>(value) & 1) != 0;
  }

  inline void* threadLocalBottom(const int tid) const
  {
    return reinterpret_cast<void*>(static_cast<uintptr_t>((tid << 1) | 1));
  }

 public:
  static constexpr size_t RING_SIZE = ring_size;

  PRQueue(uint64_t start) : Base()
  {
    for (uint64_t i = start; i < start + RING_SIZE; i++) {
      uint64_t j = i % RING_SIZE;
      array[remap[j]].val.store(nullptr, std::memory_order_relaxed);
      array[remap[j]].idx.store(i, std::memory_order_relaxed);
    }
    Base::head.store(start, std::memory_order_relaxed);
    Base::tail.store(start, std::memory_order_relaxed);
  }

  static std::string className()
  {
    using namespace std::string_literals;
    return "PRQueue"s + (padded_cells ? "/ca"s : ""s) +
           (cache_remap ? "/remap" : "");
  }

  bool enqueue(T* item, [[maybe_unused]] const int tid)
  {
    int try_close = 0;

    while (true) {
      uint64_t tailticket = Base::tail.fetch_add(1);
      if (Base::isClosed(tailticket)) {
        return false;
      }

      Cell& cell = array[remap[tailticket % RING_SIZE]];
      uint64_t idx = cell.idx.load();
      void* val = cell.val.load();
      if (val == nullptr && nodeIndex(idx) <= tailticket &&
          (!nodeUnsafe(idx) || Base::head.load() <= tailticket)) {
        void* bottom = threadLocalBottom(tid);
        if (cell.val.compare_exchange_strong(val, bottom)) {
          if (cell.idx.compare_exchange_strong(idx, tailticket + RING_SIZE)) {
            if (cell.val.compare_exchange_strong(bottom, item)) {
              return true;
            }
          } else {
            cell.val.compare_exchange_strong(bottom, nullptr);
          }
        }
      }
      if (tailticket >= Base::head.load() + RING_SIZE) {
        if (Base::closeSegment(tailticket, ++try_close > 10)) return false;
      }
    }
  }

  T* dequeue([[maybe_unused]] const int tid)
  {
#ifdef CAUTIOUS_DEQUEUE
    if (Base::isEmpty()) return nullptr;
#endif

    while (true) {
      uint64_t headticket = Base::head.fetch_add(1);
      Cell& cell = array[remap[headticket % RING_SIZE]];

      int r = 0;
      uint64_t tt = 0;

      while (true) {
        uint64_t cell_idx = cell.idx.load();
        uint64_t unsafe = nodeUnsafe(cell_idx);
        uint64_t idx = nodeIndex(cell_idx);
        void* val = cell.val.load();

        if (idx > headticket + RING_SIZE) break;

        if (val != nullptr && !isBottom(val)) {
          if (idx == headticket + RING_SIZE) {
            cell.val.store(nullptr);
            return static_cast<T*>(val);
          } else {
            if (unsafe) {
              if (cell.idx.load() == cell_idx) break;
            } else {
              if (cell.idx.compare_exchange_strong(cell_idx, setUnsafe(idx)))
                break;
            }
          }
        } else {
          if ((r & ((1ull << 8) - 1)) == 0) tt = Base::tail.load();

          int crq_closed = Base::isClosed(tt);
          uint64_t t = Base::tailIndex(tt);
          if (unsafe || t < headticket + 1 || crq_closed || r > 4 * 1024) {
            if (isBottom(val) &&
                !cell.val.compare_exchange_strong(val, nullptr))
              continue;
            if (cell.idx.compare_exchange_strong(
                    cell_idx, unsafe | (headticket + RING_SIZE)))
              break;
          }
          ++r;
        }
      }

      if (Base::tailIndex(Base::tail.load()) <= headticket + 1) {
        Base::fixState();
        return nullptr;
      }
    }
  }
};

template <typename T, bool padded_cells = false, size_t ring_size = 1024,
          bool cache_remap = true>
using LPRQueue =
    LinkedRingQueue<T, PRQueue<T, padded_cells, ring_size, cache_remap>>;
