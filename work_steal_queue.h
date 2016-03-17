#ifndef WORK_STEAL_QUEUE_H_
#define WORK_STEAL_QUEUE_H_

#include <atomic>

class work_steal_queue {
  class circular_array {
    public:
      circular_array(std::size_t n) : items(n) {}
      std::size_t size() const {
        return items.size();
      }
      void* get(std::size_t index) {
        return items[index & (size() - 1)].load(std::memory_order_relaxed);
      }
      void put(std::size_t index, void* x) {
        items[index & (size() - 1)].store(x, std::memory_order_relaxed);
      }

      circular_array* grow(std::size_t top, std::size_t bottom) {
        circular_array* new_array = new circular_array(size() * 2);
        new_array->previous.reset(this);
        for (std::size_t i = top; i != bottom; ++i)
          new_array->put(i, get(i));
        return new_array;
      }

    private:
      std::vector<std::atomic<void*>> items;
      std::unique_ptr<circular_array> previous;
  };

  std::atomic<circular_array*> array;
  std::atomic<std::size_t> top, bottom;

  public:
  work_steal_queue()
    : array(new circular_array(32)), top(0), bottom(0) {}
  ~work_steal_queue()
  {
    circular_array* p = array.load(std::memory_order_relaxed);
    if (p)
      delete p;
  }

  int size() {
    return (bottom - top);
  }

  void push(void* x)
  {
    std::size_t b = bottom.load(std::memory_order_relaxed);
    std::size_t t = top.load(std::memory_order_acquire);
    circular_array* a = array.load(std::memory_order_relaxed);
    if (b - t > a->size() - 1) {
      a = a->grow(t, b);
      array.store(a, std::memory_order_relaxed);
    }
    a->put(b, x);
    std::atomic_thread_fence(std::memory_order_release);
    bottom.store(b + 1, std::memory_order_relaxed);
  }

  void* pop()
  {
    std::size_t b = bottom.load(std::memory_order_relaxed) - 1;
    circular_array* a = array.load(std::memory_order_relaxed);
    bottom.store(b, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    std::size_t t = top.load(std::memory_order_relaxed);
    if (t <= b) {
      void* x = a->get(b);
      if (t == b) {
        if (!top.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst, std::memory_order_relaxed))
          x = nullptr;
        bottom.store(b + 1, std::memory_order_relaxed);
      }
      return x;
    } else {
      bottom.store(b + 1, std::memory_order_relaxed);
      return nullptr;
    }
  }

  void* steal()
  {
    std::size_t t = top.load(std::memory_order_acquire);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    std::size_t b = bottom.load(std::memory_order_acquire);
    void* x = nullptr;
    if (t < b) {
      circular_array* a = array.load(std::memory_order_relaxed);
      x = a->get(t);
      if (!top.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst, std::memory_order_relaxed))
        return nullptr;
    }
    return x;
  }
};

#endif
