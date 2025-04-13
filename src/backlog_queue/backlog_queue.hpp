// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#ifndef LCI_BACKLOG_QUEUE_HPP
#define LCI_BACKLOG_QUEUE_HPP

namespace lci
{
class endpoint_impl_t;
class backlog_queue_t
{
 public:
  backlog_queue_t() : empty(true), nentries_per_rank(get_nranks())
  {
    for (auto& nentries : nentries_per_rank) {
      nentries.val.store(0, std::memory_order_relaxed);
    }
  }
  ~backlog_queue_t()
  {
    LCI_Assert(backlog_queue.empty(), "backlog queue is not empty\n");
    for (auto& nentries : nentries_per_rank) {
      LCI_Assert(nentries.val.load() == 0, "nentries is not zero\n");
    }
  }
  inline void push_sends(endpoint_impl_t* endpoint, int rank, void* buffer,
                         size_t size, net_imm_data_t imm_data);
  inline void push_send(endpoint_impl_t* endpoint, int rank, void* buffer,
                        size_t size, mr_t mr, net_imm_data_t imm_data,
                        void* user_context);
  inline void push_puts(endpoint_impl_t* endpoint, int rank, void* buffer,
                        size_t size, uintptr_t base, uint64_t offset,
                        rkey_t rkey);
  inline void push_put(endpoint_impl_t* endpoint, int rank, void* buffer,
                       size_t size, mr_t mr, uintptr_t base, uint64_t offset,
                       rkey_t rkey, void* user_context);
  inline void push_putImms(endpoint_impl_t* endpoint, int rank, void* buffer,
                           size_t size, uintptr_t base, uint64_t offset,
                           rkey_t rkey, net_imm_data_t imm_data);
  inline void push_putImm(endpoint_impl_t* endpoint, int rank, void* buffer,
                          size_t size, mr_t mr, uintptr_t base, uint64_t offset,
                          rkey_t rkey, net_imm_data_t imm_data,
                          void* user_context);
  inline void push_get(endpoint_impl_t* endpoint, int rank, void* buffer,
                       size_t size, mr_t mr, uintptr_t base, uint64_t offset,
                       rkey_t rkey, void* user_context);
  inline bool progress();
  inline void set_empty(bool empty_)
  {
    if (empty.val.load(std::memory_order_relaxed) != empty_) {
      empty.val.store(empty_, std::memory_order_relaxed);
    }
  }
  inline bool is_empty() const
  {
    return empty.val.load(std::memory_order_relaxed);
  }
  inline bool is_empty(int rank) const
  {
    return nentries_per_rank[rank].val.load(std::memory_order_relaxed) == 0;
  }

 private:
  enum class backlog_op_t {
    sends,
    send,
    puts,
    put,
    putImms,
    putImm,
    get,
  };
  struct backlog_queue_entry_t {
    backlog_op_t op;
    endpoint_impl_t* endpoint;
    int rank;
    void* buffer;
    size_t size;
    mr_t mr;
    uintptr_t base;
    uint64_t offset;
    rkey_t rkey;
    net_imm_data_t imm_data;
    void* user_context;
  };
  // we use a lock-based queue instead of a atomic-based queue for two reasons:
  // 1. we assume that the backlog queue is not used frequently.
  // 2. we would like to ensure the operations are executed in the order they
  // are pushed.
  padded_atomic_t<bool> empty;
  std::vector<padded_atomic_t<size_t>> nentries_per_rank;
  LCIU_CACHE_PADDING(0);
  spinlock_t lock;
  LCIU_CACHE_PADDING(sizeof(spinlock_t));
  std::queue<backlog_queue_entry_t> backlog_queue;
};

}  // namespace lci

#endif  // LCI_BACKLOG_QUEUE_HPP