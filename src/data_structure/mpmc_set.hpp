#ifndef LCI_MPMC_SET_HPP
#define LCI_MPMC_SET_HPP

namespace lci
{
class mpmc_set_t
{
  struct alignas(LCI_CACHE_LINE) local_set_t {
    int id;
    std::vector<void*> queue;
    int64_t head, tail;
    spinlock_t lock;
    static void copy_to_new(local_set_t* src, std::vector<void*>& new_queue,
                            int64_t n)
    {
      LCI_Assert(src->size() >= n,
                 "Insufficient packets in source pool: %ld (need %ld)\n",
                 src->size(), n);
      LCI_Assert(new_queue.size() > n,
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
    local_set_t(int64_t default_size = 1024) : head(0), tail(0)
    {
      LCI_Assert(default_size > 0, "default_size must be positive");
      queue.resize(default_size + 1, nullptr);
    }
    void push(void* packet)
    {
      LCI_Assert(packet, "push found a nullptr\n");
      int64_t current_size = size();
      if (current_size == queue.size() - 1) {
        // resize the queue
        std::vector<void*> new_queue(queue.size() * 2);
        local_set_t::copy_to_new(this, new_queue, current_size);
        head = 0;
        tail = current_size;
        LCI_Assert(head < tail, "header (%ld) > tail (%ld)\n", head, tail);
        queue.swap(new_queue);
        for (int i = 0; i < current_size; i++) {
          void* val = queue[i];
          LCI_Assert(val, "val must not be nullptr\n");
        }
      }
      queue[tail++ % queue.size()] = packet;
    }
    void* pop()
    {
      if (head == tail) {
        return nullptr;
      }
      LCI_Assert(head < tail, "header (%ld) > tail (%ld)\n", head, tail);
      void* ret = queue[head++ % queue.size()];
      LCI_Assert(ret, "pop a nullptr\n");
      return ret;
    }
    bool empty() const { return head == tail; }
    int64_t size() const { return tail - head; }
    int64_t capacity() const { return queue.size() - 1; }
  };

 public:
  mpmc_set_t(int default_nthreads = 256, int64_t default_lpool_size_ = 1024)
      : npools(0),
        pools(default_nthreads),
        tid_to_pools(default_nthreads),
        default_lpool_size(default_lpool_size_)
  {
  }
  ~mpmc_set_t()
  {
    for (int i = 0; i < npools; i++) {
      delete static_cast<local_set_t*>(pools.get(i));
    }
  };
  const static int LOCAL_SET_ID_NULL = -1;
  int get_local_set_id() { return LCT_get_thread_id(); }
  void* get();
  void put(void* packet, int tid);
  // not thread safe
  int64_t size() const;

 private:
  local_set_t* get_local_pool();
  local_set_t* get_random_pool();

  int64_t default_lpool_size;
  std::atomic<int> npools;
  mpmc_array_t pools;
  mpmc_array_t tid_to_pools;
};

inline mpmc_set_t::local_set_t* mpmc_set_t::get_local_pool()
{
  int tid = get_local_set_id();
  void* ptr = tid_to_pools.get(tid);
  if (LCT_likely(ptr)) {
    return static_cast<local_set_t*>(ptr);
  }
  // we need to allocate a new one
  ptr = new local_set_t(default_lpool_size);
  int pool_id = npools++;
  static_cast<local_set_t*>(ptr)->id = pool_id;
  pools.put(pool_id, ptr);
  tid_to_pools.put(tid, ptr);
  return static_cast<local_set_t*>(ptr);
}

inline mpmc_set_t::local_set_t* mpmc_set_t::get_random_pool()
{
  int pool_id = rand_mt() % npools;
  return static_cast<local_set_t*>(pools.get(pool_id));
}

inline void* mpmc_set_t::get()
{
  int tid = get_local_set_id();
  local_set_t* local_pool = get_local_pool();
  local_set_t* random_pool;
  int64_t n;

  if (!local_pool->lock.try_lock()) {
    return nullptr;
  }
  void* ret = local_pool->pop();
  if (ret) {
    goto unlock_local_pool;
  }
  // random packet stealing
  LCI_Assert(local_pool->empty(), "local pool must be empty\n");
  random_pool = get_random_pool();
  if (!random_pool || random_pool == local_pool || random_pool->size() <= 0) {
    goto unlock_local_pool;
  }
  if (!random_pool->lock.try_lock()) goto unlock_local_pool;
  // At this point, we should have locked both pools
  // steal half packets from random_pool
  n = (random_pool->size() + 1) / 2;
  if (n > 0) {
    LCI_Assert(n <= random_pool->size(),
               "n (%ld) > random_pool->size() (%ld)\n", n, random_pool->size());
    local_pool->queue.resize(std::max(default_lpool_size, n * 2) + 1);
    local_set_t::copy_to_new(random_pool, local_pool->queue, n);
    LCI_Assert(random_pool->head <= random_pool->tail,
               "header (%ld) > tail (%ld)\n", random_pool->head,
               random_pool->tail);
    local_pool->tail = n;
    local_pool->head = 0;
    for (int i = 0; i < n; i++) {
      void* val = local_pool->queue[i];
      LCI_Assert(val, "val must not be nullptr\n");
    }
    ret = local_pool->pop();
    LCI_Assert(ret, "this pop has to succeed\n");
  }

unlock_random_pool:
  random_pool->lock.unlock();

unlock_local_pool:
  local_pool->lock.unlock();
  return ret;
}

inline void mpmc_set_t::put(void* packet,
                            int tid = mpmc_set_t::LOCAL_SET_ID_NULL)
{
  LCI_Assert(packet, "packet must not be nullptr\n");
  if (tid < 0) {
    tid = get_local_set_id();
  }
  local_set_t* local_pool = get_local_pool();

  local_pool->lock.lock();
  local_pool->push(packet);
  local_pool->lock.unlock();
}

inline int64_t mpmc_set_t::size() const
{
  int64_t total = 0;
  for (int i = 0; i < npools; i++) {
    local_set_t* pool = static_cast<local_set_t*>(pools.get(i));
    LCI_Assert(pool, "pool must not be nullptr\n");
    total += pool->size();
  }
  return total;
}
}  // namespace lci

#endif  // LCI_MPMC_SET_HPP