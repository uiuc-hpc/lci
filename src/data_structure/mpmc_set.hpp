#ifndef LCI_MPMC_SET_HPP
#define LCI_MPMC_SET_HPP

namespace lci
{
// The packet pool is implemented as a multiple-producer multiple-consumer set
// consisting of a deque per thread, protected by a spinlock.
// For better cache locality, we define the following rules:
// - Normal push and pop happens at the bottom of the deque.
// - Cross-thread push and packet steal happens at the top of the deque.
class mpmc_set_t
{
  class alignas(LCI_CACHE_LINE) local_set_t
  {
   public:
    local_set_t(int64_t default_size = 1024) : head(0), tail(0)
    {
      LCI_Assert(default_size > 0, "default_size must be positive");
      queue.resize(default_size + 1, nullptr);
    }

    void expand()
    {
      int64_t current_size = size();
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

    void push_to_bot(void* packet)
    {
      LCI_Assert(packet, "push found a nullptr\n");
      if (size() == queue.size() - 1) {
        expand();
      }
      queue[get_position(tail++)] = packet;
    }

    void push_to_top(void* packet)
    {
      LCI_Assert(packet, "push found a nullptr\n");
      if (size() == queue.size() - 1) {
        expand();
      }
      queue[get_position(--head)] = packet;
    }

    void* pop_from_top()
    {
      if (head == tail) {
        return nullptr;
      }
      LCI_Assert(head < tail, "header (%ld) > tail (%ld)\n", head, tail);
      void* ret = queue[get_position(head++)];
      LCI_Assert(ret, "pop a nullptr\n");
      return ret;
    }

    void* pop_from_bot()
    {
      if (head == tail) {
        return nullptr;
      }
      LCI_Assert(head < tail, "header (%ld) > tail (%ld)\n", head, tail);
      void* ret = queue[get_position(--tail)];
      LCI_Assert(ret, "pop a nullptr\n");
      return ret;
    }

    bool empty() const { return head == tail; }
    int64_t size() const { return tail - head; }
    int64_t capacity() const { return queue.size() - 1; }

    int id;
    std::vector<void*> queue;
    int64_t head;  // head is the index of the first element
    int64_t tail;  // tail is the index of the next available slot
    spinlock_t lock;

    // modulo operation that always returns a positive number [0, queue.size())
    int64_t get_position(int64_t idx)
    {
      int64_t size = queue.size();
      return (idx % size + size) % size;
    }

    // copy from the top of the source pool to the destination queue
    static void copy_to_new(local_set_t* src, std::vector<void*>& new_queue,
                            int64_t n)
    {
      LCI_Assert(src->size() >= n,
                 "Insufficient packets in source pool: %ld (need %ld)\n",
                 src->size(), n);
      LCI_Assert(new_queue.size() > n,
                 "Insufficient size in destination queue: %lu (need %ld)\n",
                 new_queue.size(), n);
      int64_t head_idx = src->get_position(src->head);
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
  int get_local_set_id() const { return LCT_get_thread_id(); }
  void* get(int64_t max_steal_attempts);
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

inline void* mpmc_set_t::get(int64_t max_steal_attempts = 1)
{
  int tid = get_local_set_id();
  local_set_t* local_pool = get_local_pool();
  local_set_t* random_pool;
  int64_t n;

  if (!local_pool->lock.try_lock()) {
    return nullptr;
  }
  void* ret = local_pool->pop_from_bot();
  if (ret) {
    goto unlock_local_pool;
  }
  // random packet stealing
  // we steal from the top of the random pool
  for (int64_t i = 0; i < max_steal_attempts; i++) {
    LCI_Assert(local_pool->empty(), "local pool must be empty\n");
    random_pool = get_random_pool();
    if (!random_pool || random_pool == local_pool || random_pool->size() <= 0) {
      continue;
    }
    if (!random_pool->lock.try_lock()) continue;
    // At this point, we should have locked both pools
    n = (random_pool->size() + 1) / 2;
    if (n > 0) {
      LCI_Assert(n <= random_pool->size(),
                 "n (%ld) > random_pool->size() (%ld)\n", n,
                 random_pool->size());
      local_pool->queue.resize(std::max(default_lpool_size, n * 2) + 1);
      // steal half packets from random_pool
      local_set_t::copy_to_new(random_pool, local_pool->queue, n);
      LCI_Assert(random_pool->head <= random_pool->tail,
                 "header (%ld) > tail (%ld)\n", random_pool->head,
                 random_pool->tail);
      local_pool->tail = n;
      local_pool->head = 0;
#ifdef LCI_DEBUG
      for (int i = 0; i < n; i++) {
        void* val = local_pool->queue[i];
        LCI_DBG_Assert(val, "val must not be nullptr\n");
      }
#endif
      ret = local_pool->pop_from_bot();
      LCI_Assert(ret, "this pop has to succeed\n");
    }
    random_pool->lock.unlock();
    // if we get a packet, we are done
    if (ret) {
      break;
    }
  }

unlock_local_pool:
  local_pool->lock.unlock();
  return ret;
}

inline void mpmc_set_t::put(void* packet,
                            int tid = mpmc_set_t::LOCAL_SET_ID_NULL)
{
  LCI_Assert(packet, "packet must not be nullptr\n");
  bool return_to_current = false;
  if (tid < 0) {
    // return to the local pool of the current thread
    tid = get_local_set_id();
    return_to_current = true;
  }
  local_set_t* local_pool = get_local_pool();

  local_pool->lock.lock();
  if (return_to_current) {
    // we push to the bot for better cacahe locality
    local_pool->push_to_bot(packet);
  } else {
    local_pool->push_to_top(packet);
  }
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