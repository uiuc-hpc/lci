// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#ifndef LCI_BACKLOG_QUEUE_INLINE_HPP
#define LCI_BACKLOG_QUEUE_INLINE_HPP

namespace lci
{
inline void backlog_queue_t::push_sends(endpoint_impl_t* endpoint, int rank,
                                        void* buffer, size_t size,
                                        net_imm_data_t imm_data,
                                        void* user_context)
{
  LCI_PCOUNTER_ADD(backlog_queue_push, 1);
  backlog_queue_entry_t entry;
  entry.op = backlog_op_t::sends;
  entry.endpoint = endpoint;
  entry.rank = rank;
  if (size) {
    entry.buffer = malloc(size);
    memcpy(entry.buffer, buffer, size);
  } else {
    entry.buffer = nullptr;
  }
  entry.size = size;
  entry.imm_data = imm_data;
  entry.user_context = user_context;

  nentries_per_rank[rank].val.fetch_add(1, std::memory_order_relaxed);
  lock.lock();
  backlog_queue.push(entry);
  set_empty(false);  // we need to be careful not to lose the signal
  lock.unlock();
}

inline void backlog_queue_t::push_send(endpoint_impl_t* endpoint, int rank,
                                       void* buffer, size_t size, mr_t mr,
                                       net_imm_data_t imm_data,
                                       void* user_context)
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
  entry.user_context = user_context;

  nentries_per_rank[rank].val.fetch_add(1, std::memory_order_relaxed);
  lock.lock();
  backlog_queue.push(entry);
  set_empty(false);
  lock.unlock();
}

inline void backlog_queue_t::push_puts(endpoint_impl_t* endpoint, int rank,
                                       void* buffer, size_t size,
                                       uint64_t offset, rmr_t rmr,
                                       void* user_context)
{
  LCI_PCOUNTER_ADD(backlog_queue_push, 1);
  backlog_queue_entry_t entry;
  entry.op = backlog_op_t::puts;
  entry.endpoint = endpoint;
  entry.rank = rank;
  entry.buffer = malloc(size);
  memcpy(entry.buffer, buffer, size);
  entry.size = size;
  entry.offset = offset;
  entry.rmr = rmr;
  entry.user_context = user_context;

  nentries_per_rank[rank].val.fetch_add(1, std::memory_order_relaxed);
  lock.lock();
  backlog_queue.push(entry);
  set_empty(false);
  lock.unlock();
}

inline void backlog_queue_t::push_put(endpoint_impl_t* endpoint, int rank,
                                      void* buffer, size_t size, mr_t mr,
                                      uint64_t offset, rmr_t rmr,
                                      void* user_context)
{
  LCI_PCOUNTER_ADD(backlog_queue_push, 1);
  backlog_queue_entry_t entry;
  entry.op = backlog_op_t::put;
  entry.endpoint = endpoint;
  entry.rank = rank;
  entry.buffer = buffer;
  entry.size = size;
  entry.mr = mr;
  entry.offset = offset;
  entry.rmr = rmr;
  entry.user_context = user_context;

  nentries_per_rank[rank].val.fetch_add(1, std::memory_order_relaxed);
  lock.lock();
  backlog_queue.push(entry);
  set_empty(false);
  lock.unlock();
}

inline void backlog_queue_t::push_putImms(endpoint_impl_t* endpoint, int rank,
                                          void* buffer, size_t size,
                                          uint64_t offset, rmr_t rmr,
                                          net_imm_data_t imm_data,
                                          void* user_context)
{
  LCI_PCOUNTER_ADD(backlog_queue_push, 1);
  backlog_queue_entry_t entry;
  entry.op = backlog_op_t::putImms;
  entry.endpoint = endpoint;
  entry.rank = rank;
  entry.buffer = malloc(size);
  memcpy(entry.buffer, buffer, size);
  entry.size = size;
  entry.offset = offset;
  entry.rmr = rmr;
  entry.imm_data = imm_data;
  entry.user_context = user_context;

  nentries_per_rank[rank].val.fetch_add(1, std::memory_order_relaxed);
  lock.lock();
  backlog_queue.push(entry);
  set_empty(false);
  lock.unlock();
}
inline void backlog_queue_t::push_putImm(endpoint_impl_t* endpoint, int rank,
                                         void* buffer, size_t size, mr_t mr,
                                         uint64_t offset, rmr_t rmr,
                                         net_imm_data_t imm_data,
                                         void* user_context)
{
  LCI_PCOUNTER_ADD(backlog_queue_push, 1);
  backlog_queue_entry_t entry;
  entry.op = backlog_op_t::putImm;
  entry.endpoint = endpoint;
  entry.rank = rank;
  entry.buffer = buffer;
  entry.size = size;
  entry.mr = mr;
  entry.offset = offset;
  entry.rmr = rmr;
  entry.imm_data = imm_data;
  entry.user_context = user_context;

  nentries_per_rank[rank].val.fetch_add(1, std::memory_order_relaxed);
  lock.lock();
  backlog_queue.push(entry);
  set_empty(false);
  lock.unlock();
}

inline void backlog_queue_t::push_get(endpoint_impl_t* endpoint, int rank,
                                      void* buffer, size_t size, mr_t mr,
                                      uint64_t offset, rmr_t rmr,
                                      void* user_context)
{
  LCI_PCOUNTER_ADD(backlog_queue_push, 1);
  backlog_queue_entry_t entry;
  entry.op = backlog_op_t::get;
  entry.endpoint = endpoint;
  entry.rank = rank;
  entry.buffer = buffer;
  entry.size = size;
  entry.mr = mr;
  entry.offset = offset;
  entry.rmr = rmr;
  entry.user_context = user_context;

  nentries_per_rank[rank].val.fetch_add(1, std::memory_order_relaxed);
  lock.lock();
  backlog_queue.push(entry);
  set_empty(false);
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
  if (backlog_queue.empty()) {
    set_empty(true);
    lock.unlock();
    return false;
  }
  bool did_something = false;
  auto entry = backlog_queue.front();
  error_t error;
  switch (entry.op) {
    case backlog_op_t::sends:
      error = entry.endpoint->post_sends(entry.rank, entry.buffer, entry.size,
                                         entry.imm_data, entry.user_context,
                                         true, true);
      break;
    case backlog_op_t::send:
      error = entry.endpoint->post_send(entry.rank, entry.buffer, entry.size,
                                        entry.mr, entry.imm_data,
                                        entry.user_context, true, true);
      break;
    case backlog_op_t::puts:
      error = entry.endpoint->post_puts(entry.rank, entry.buffer, entry.size,
                                        entry.offset, entry.rmr,
                                        entry.user_context, true, true);
      break;
    case backlog_op_t::put:
      error = entry.endpoint->post_put(entry.rank, entry.buffer, entry.size,
                                       entry.mr, entry.offset, entry.rmr,
                                       entry.user_context, true, true);
      break;
    case backlog_op_t::putImms:
      error = entry.endpoint->post_putImms(
          entry.rank, entry.buffer, entry.size, entry.offset, entry.rmr,
          entry.imm_data, entry.user_context, true, true);
      break;
    case backlog_op_t::putImm:
      error = entry.endpoint->post_putImm(
          entry.rank, entry.buffer, entry.size, entry.mr, entry.offset,
          entry.rmr, entry.imm_data, entry.user_context, true, true);
      break;
    case backlog_op_t::get:
      error = entry.endpoint->post_get(entry.rank, entry.buffer, entry.size,
                                       entry.mr, entry.offset, entry.rmr,
                                       entry.user_context, true, true);
      break;
    default:
      LCI_Assert(false, "Unknown operation %d\n", entry.op);
  }
  if (!error.is_retry()) {
    LCI_DBG_Log(LOG_TRACE, "network", "backlog_queue progress rank %d op %d\n",
                entry.rank, entry.op);
    backlog_queue.pop();
    if (backlog_queue.empty()) {
      set_empty(true);
    }
    lock.unlock();
    // clean up
    nentries_per_rank[entry.rank].val.fetch_sub(1, std::memory_order_relaxed);
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
