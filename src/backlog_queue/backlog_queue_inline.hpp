#ifndef LCI_BACKLOG_QUEUE_INLINE_HPP
#define LCI_BACKLOG_QUEUE_INLINE_HPP

namespace lci
{
inline void backlog_queue_t::push_sends(endpoint_impl_t* endpoint, int rank,
                                        void* buffer, size_t size,
                                        net_imm_data_t imm_data)
{
  LCI_PCOUNTER_ADD(backlog_queue_push, 1);
  backlog_queue_entry_t entry;
  entry.op = backlog_op_t::sends;
  entry.endpoint = endpoint;
  entry.rank = rank;
  entry.buffer = malloc(size);
  memcpy(entry.buffer, buffer, size);
  entry.size = size;
  entry.imm_data = imm_data;
  lock.lock();
  nentries.fetch_add(1, std::memory_order::memory_order_relaxed);
  backlog_queue.push(entry);
  lock.unlock();
}

inline void backlog_queue_t::push_send(endpoint_impl_t* endpoint, int rank,
                                       void* buffer, size_t size, mr_t mr,
                                       net_imm_data_t imm_data, void* ctx)
{
  LCI_PCOUNTER_ADD(backlog_queue_push, 1);
  backlog_queue_entry_t entry;
  entry.op = backlog_op_t::send;
  entry.endpoint = endpoint;
  entry.rank = rank;
  entry.buffer = buffer;
  entry.size = size;
  entry.mr = mr;
  entry.imm_data = imm_data;
  entry.ctx = ctx;
  lock.lock();
  nentries.fetch_add(1, std::memory_order::memory_order_relaxed);
  backlog_queue.push(entry);
  lock.unlock();
}

inline void backlog_queue_t::push_puts(endpoint_impl_t* endpoint, int rank,
                                       void* buffer, size_t size,
                                       uintptr_t base, uint64_t offset,
                                       rkey_t rkey)
{
  LCI_PCOUNTER_ADD(backlog_queue_push, 1);
  backlog_queue_entry_t entry;
  entry.op = backlog_op_t::puts;
  entry.endpoint = endpoint;
  entry.rank = rank;
  entry.buffer = malloc(size);
  memcpy(entry.buffer, buffer, size);
  entry.size = size;
  entry.base = base;
  entry.offset = offset;
  entry.rkey = rkey;
  lock.lock();
  nentries.fetch_add(1, std::memory_order::memory_order_relaxed);
  backlog_queue.push(entry);
  lock.unlock();
}

inline void backlog_queue_t::push_put(endpoint_impl_t* endpoint, int rank,
                                      void* buffer, size_t size, mr_t mr,
                                      uintptr_t base, uint64_t offset,
                                      rkey_t rkey, void* ctx)
{
  LCI_PCOUNTER_ADD(backlog_queue_push, 1);
  backlog_queue_entry_t entry;
  entry.op = backlog_op_t::put;
  entry.endpoint = endpoint;
  entry.rank = rank;
  entry.buffer = buffer;
  entry.size = size;
  entry.mr = mr;
  entry.base = base;
  entry.offset = offset;
  entry.rkey = rkey;
  entry.ctx = ctx;
  lock.lock();
  nentries.fetch_add(1, std::memory_order::memory_order_relaxed);
  backlog_queue.push(entry);
  lock.unlock();
}

inline void backlog_queue_t::push_putImms(endpoint_impl_t* endpoint, int rank,
                                          void* buffer, size_t size,
                                          uintptr_t base, uint64_t offset,
                                          rkey_t rkey, net_imm_data_t imm_data)
{
  LCI_PCOUNTER_ADD(backlog_queue_push, 1);
  backlog_queue_entry_t entry;
  entry.op = backlog_op_t::putImms;
  entry.endpoint = endpoint;
  entry.rank = rank;
  entry.buffer = malloc(size);
  memcpy(entry.buffer, buffer, size);
  entry.size = size;
  entry.base = base;
  entry.offset = offset;
  entry.rkey = rkey;
  entry.imm_data = imm_data;
  lock.lock();
  nentries.fetch_add(1, std::memory_order::memory_order_relaxed);
  backlog_queue.push(entry);
  lock.unlock();
}
inline void backlog_queue_t::push_putImm(endpoint_impl_t* endpoint, int rank,
                                         void* buffer, size_t size, mr_t mr,
                                         uintptr_t base, uint64_t offset,
                                         rkey_t rkey, net_imm_data_t imm_data,
                                         void* ctx)
{
  LCI_PCOUNTER_ADD(backlog_queue_push, 1);
  backlog_queue_entry_t entry;
  entry.op = backlog_op_t::putImm;
  entry.endpoint = endpoint;
  entry.rank = rank;
  entry.buffer = buffer;
  entry.size = size;
  entry.mr = mr;
  entry.base = base;
  entry.offset = offset;
  entry.rkey = rkey;
  entry.imm_data = imm_data;
  entry.ctx = ctx;
  lock.lock();
  nentries.fetch_add(1, std::memory_order::memory_order_relaxed);
  backlog_queue.push(entry);
  lock.unlock();
}

inline void backlog_queue_t::push_get(endpoint_impl_t* endpoint, int rank,
                                      void* buffer, size_t size, mr_t mr,
                                      uintptr_t base, uint64_t offset,
                                      rkey_t rkey, void* ctx)
{
  LCI_PCOUNTER_ADD(backlog_queue_push, 1);
  backlog_queue_entry_t entry;
  entry.op = backlog_op_t::get;
  entry.endpoint = endpoint;
  entry.rank = rank;
  entry.buffer = buffer;
  entry.size = size;
  entry.mr = mr;
  entry.base = base;
  entry.offset = offset;
  entry.rkey = rkey;
  entry.ctx = ctx;
  lock.lock();
  nentries.fetch_add(1, std::memory_order::memory_order_relaxed);
  backlog_queue.push(entry);
  lock.unlock();
}

inline bool backlog_queue_t::progress()
{
  if (is_empty()) {
    return false;
  }
  if (!lock.try_lock()) {
    return false;
  }
  bool did_something = false;
  auto entry = backlog_queue.front();
  error_t error;
  switch (entry.op) {
    case backlog_op_t::sends:
      error = entry.endpoint->post_sends(entry.rank, entry.buffer, entry.size,
                                         entry.imm_data, true, true);
      break;
    case backlog_op_t::send:
      error = entry.endpoint->post_send(entry.rank, entry.buffer, entry.size,
                                        entry.mr, entry.imm_data, entry.ctx,
                                        true, true);
      break;
    case backlog_op_t::puts:
      error = entry.endpoint->post_puts(entry.rank, entry.buffer, entry.size,
                                        entry.base, entry.offset, entry.rkey,
                                        true, true);
      break;
    case backlog_op_t::put:
      error = entry.endpoint->post_put(entry.rank, entry.buffer, entry.size,
                                       entry.mr, entry.base, entry.offset,
                                       entry.rkey, entry.ctx, true, true);
      break;
    case backlog_op_t::putImms:
      error = entry.endpoint->post_putImms(entry.rank, entry.buffer, entry.size,
                                           entry.base, entry.offset, entry.rkey,
                                           entry.imm_data, true, true);
      break;
    case backlog_op_t::putImm:
      error = entry.endpoint->post_putImm(
          entry.rank, entry.buffer, entry.size, entry.mr, entry.base,
          entry.offset, entry.rkey, entry.imm_data, entry.ctx, true, true);
      break;
    case backlog_op_t::get:
      error = entry.endpoint->post_get(entry.rank, entry.buffer, entry.size,
                                       entry.mr, entry.base, entry.offset,
                                       entry.rkey, entry.ctx, true, true);
      break;
    default:
      LCI_Assert(false, "Unknown operation %d\n", entry.op);
  }
  if (!error.is_retry()) {
    backlog_queue.pop();
    lock.unlock();
    // clean up
    nentries.fetch_sub(1, std::memory_order::memory_order_relaxed);
    did_something = true;
    if (entry.op == backlog_op_t::sends || entry.op == backlog_op_t::puts ||
        entry.op == backlog_op_t::putImms) {
      free(entry.buffer);
    }
    LCI_PCOUNTER_ADD(backlog_queue_pop, 1);
  } else {
    lock.unlock();
  }
  return did_something;
}

}  // namespace lci

#endif  // LCI_BACKLOG_QUEUE_INLINE_HPP