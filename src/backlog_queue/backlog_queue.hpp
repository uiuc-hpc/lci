#ifndef LCI_BACKLOG_QUEUE_HPP
#define LCI_BACKLOG_QUEUE_HPP

namespace lci
{
class endpoint_impl_t;
class backlog_queue_t
{
 public:
  backlog_queue_t() : nentries(0) {}
  ~backlog_queue_t() = default;
  inline void push_sends(endpoint_impl_t* endpoint, int rank, void* buffer,
                         size_t size, net_imm_data_t imm_data);
  inline void push_send(endpoint_impl_t* endpoint, int rank, void* buffer,
                        size_t size, mr_t mr, net_imm_data_t imm_data,
                        void* ctx);
  inline void push_puts(endpoint_impl_t* endpoint, int rank, void* buffer,
                        size_t size, uintptr_t base, uint64_t offset,
                        rkey_t rkey);
  inline void push_put(endpoint_impl_t* endpoint, int rank, void* buffer,
                       size_t size, mr_t mr, uintptr_t base, uint64_t offset,
                       rkey_t rkey, void* ctx);
  inline void push_putImms(endpoint_impl_t* endpoint, int rank, void* buffer,
                           size_t size, uintptr_t base, uint64_t offset,
                           rkey_t rkey, net_imm_data_t imm_data);
  inline void push_putImm(endpoint_impl_t* endpoint, int rank, void* buffer,
                          size_t size, mr_t mr, uintptr_t base, uint64_t offset,
                          rkey_t rkey, net_imm_data_t imm_data, void* ctx);
  inline void push_get(endpoint_impl_t* endpoint, int rank, void* buffer,
                       size_t size, mr_t mr, uintptr_t base, uint64_t offset,
                       rkey_t rkey, void* ctx);
  inline bool progress();
  inline bool is_empty() const
  {
    return nentries.load(std::memory_order_relaxed) == 0;
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
    void* ctx;
  };
  // we use a lock-based queue instead of a atomic-based queue for two reasons:
  // 1. we assume that the backlog queue is not used frequently.
  // 2. we would like to ensure the operations are executed in the order they
  // are pushed.
  std::atomic<size_t> nentries;
  spinlock_t lock;
  std::queue<backlog_queue_entry_t> backlog_queue;
};

}  // namespace lci

#endif  // LCI_BACKLOG_QUEUE_HPP