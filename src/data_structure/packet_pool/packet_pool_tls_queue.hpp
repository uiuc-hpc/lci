#ifndef LCIXX_PACKET_POOL_TLS_QUEUE_HPP
#define LCIXX_PACKET_POOL_TLS_QUEUE_HPP

namespace lcixx
{
class packet_pool_tls_queue_t : public packet_pool_impl_t
{
  struct alignas(LCIXX_CACHE_LINE) local_pool_t {
    int id;
    std::vector<void*> queue;
    int64_t head, tail;
    spinlock_t lock;
    static void copy_to_new(local_pool_t* src, std::vector<void*>& new_queue,
                            int64_t n)
    {
      LCIXX_Assert(src->size() >= n,
                   "Insufficient packets in source pool: %ld (need %ld)\n",
                   src->size(), n);
      LCIXX_Assert(new_queue.size() > n,
                   "Insufficient size in destination queue: %lu (need %ld)\n",
                   new_queue.size(), n);
      int64_t head_idx = src->head % src->queue.size();
      if (head_idx + n <= src->queue.size()) {
        // no wrap around
        memcpy(new_queue.data(), &src->queue[head_idx], n * sizeof(void*));
      } else {
        int64_t first_segment_size = src->queue.size() - head_idx;
        memcpy(new_queue.data(), &src->queue[head_idx],
               first_segment_size * sizeof(void*));
        memcpy(new_queue.data() + first_segment_size, &src->queue[0],
               (n - first_segment_size) * sizeof(void*));
      }
      src->head += n;
    }
    local_pool_t(int64_t default_size = 1024) : head(0), tail(0)
    {
      LCIXX_Assert(default_size > 0, "default_size must be positive");
      queue.resize(default_size + 1, nullptr);
    }
    void push(void* packet)
    {
      LCIXX_Assert(packet, "push found a nullptr\n");
      int64_t current_size = size();
      if (current_size == queue.size() - 1) {
        // resize the queue
        std::vector<void*> new_queue(queue.size() * 2);
        local_pool_t::copy_to_new(this, new_queue, current_size);
        head = 0;
        tail = current_size;
        LCIXX_Assert(head < tail, "header (%ld) > tail (%ld)\n", head, tail);
        queue.swap(new_queue);
        for (int i = 0; i < current_size; i++) {
          void* val = queue[i];
          LCIXX_Assert(val, "val must not be nullptr\n");
        }
      }
      queue[tail++ % queue.size()] = packet;
    }
    void* pop()
    {
      if (head == tail) {
        return nullptr;
      }
      LCIXX_Assert(head < tail, "header (%ld) > tail (%ld)\n", head, tail);
      void* ret = queue[head++ % queue.size()];
      LCIXX_Assert(ret, "pop a nullptr\n");
      return ret;
    }
    bool empty() const { return head == tail; }
    int64_t size() const { return tail - head; }
    int64_t capacity() const { return queue.size() - 1; }
  };

 public:
  packet_pool_tls_queue_t(int default_nthreads = 256,
                          int64_t default_lpool_size_ = 1024)
      : npools(0),
        pools(default_nthreads),
        tid_to_pools(default_nthreads),
        default_lpool_size(default_lpool_size_)
  {
  }
  ~packet_pool_tls_queue_t() override
  {
    for (int i = 0; i < npools; i++) {
      delete static_cast<local_pool_t*>(pools.get(i));
    }
  };
  void* get() override;
  void put(void*) override;

 private:
  local_pool_t* get_local_pool();
  local_pool_t* get_random_pool();

  int64_t default_lpool_size;
  std::atomic<int> npools;
  mpmc_array_t pools;
  mpmc_array_t tid_to_pools;
};

inline packet_pool_tls_queue_t::local_pool_t*
packet_pool_tls_queue_t::get_local_pool()
{
  int tid = LCT_get_thread_id();
  void* ptr = tid_to_pools.get(tid);
  if (LCT_likely(ptr)) {
    return static_cast<local_pool_t*>(ptr);
  }
  // we need to allocate a new one
  ptr = new local_pool_t(default_lpool_size);
  int pool_id = npools++;
  static_cast<local_pool_t*>(ptr)->id = pool_id;
  pools.put(pool_id, ptr);
  tid_to_pools.put(tid, ptr);
  return static_cast<local_pool_t*>(ptr);
}

inline packet_pool_tls_queue_t::local_pool_t*
packet_pool_tls_queue_t::get_random_pool()
{
  int pool_id = rand_mt() % npools;
  return static_cast<local_pool_t*>(pools.get(pool_id));
}

inline void* packet_pool_tls_queue_t::get()
{
  int tid = LCT_get_thread_id();
  local_pool_t* local_pool = get_local_pool();
  local_pool_t* random_pool;
  int64_t n;

  if (!local_pool->lock.try_lock()) {
    return nullptr;
  }
  void* ret = local_pool->pop();
  if (ret) {
    goto unlock_local_pool;
  }
  // random packet stealing
  LCIXX_Assert(local_pool->empty(), "local pool must be empty\n");
  random_pool = get_random_pool();
  if (!random_pool || random_pool == local_pool || random_pool->size() <= 0) {
    goto unlock_local_pool;
  }
  if (!random_pool->lock.try_lock()) goto unlock_local_pool;
  // At this point, we should have locked both pools
  // steal half packets from random_pool
  n = (random_pool->size() + 1) / 2;
  if (n > 0) {
    LCIXX_Assert(n <= random_pool->size(),
                 "n (%ld) > random_pool->size() (%ld)\n", n,
                 random_pool->size());
    local_pool->queue.resize(std::max(default_lpool_size, n * 2) + 1);
    local_pool_t::copy_to_new(random_pool, local_pool->queue, n);
    LCIXX_Assert(random_pool->head <= random_pool->tail,
                 "header (%ld) > tail (%ld)\n", random_pool->head,
                 random_pool->tail);
    local_pool->tail = n;
    local_pool->head = 0;
    for (int i = 0; i < n; i++) {
      void* val = local_pool->queue[i];
      LCIXX_Assert(val, "val must not be nullptr\n");
    }
    ret = local_pool->pop();
    LCIXX_Assert(ret, "this pop has to succeed\n");
  }

unlock_random_pool:
  random_pool->lock.unlock();

unlock_local_pool:
  local_pool->lock.unlock();
  return ret;
}

inline void packet_pool_tls_queue_t::put(void* packet)
{
  LCIXX_Assert(packet, "packet must not be nullptr\n");
  int tid = LCT_get_thread_id();
  local_pool_t* local_pool = get_local_pool();

  local_pool->lock.lock();
  local_pool->push(packet);
  local_pool->lock.unlock();
}
}  // namespace lcixx

#endif  // LCIXX_PACKET_POOL_TLS_QUEUE_HPP