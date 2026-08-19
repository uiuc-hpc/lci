// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#ifndef LCI_ENDPOINT_INLINE_HPP
#define LCI_ENDPOINT_INLINE_HPP

namespace lci
{
inline bool is_valid_uint64_atomic_remote_address(uint64_t offset, rmr_t rmr)
{
  constexpr size_t ATOMIC_SIZE = sizeof(uint64_t);
  if (rmr.base % ATOMIC_SIZE != 0 || offset % ATOMIC_SIZE != 0) {
    return false;
  }
  return offset <= std::numeric_limits<uintptr_t>::max() - rmr.base;
}

inline bool is_valid_uint64_atomic_result(uint64_t* result, mr_t result_mr,
                                          device_t device)
{
  constexpr size_t ATOMIC_SIZE = sizeof(uint64_t);
  if (result == nullptr || result_mr.is_empty()) {
    return false;
  }
  mr_impl_t* result_mr_impl = result_mr.get_impl();
  if (!(result_mr_impl->device == device)) {
    return false;
  }
  uintptr_t result_address = reinterpret_cast<uintptr_t>(result);
  uintptr_t mr_address = reinterpret_cast<uintptr_t>(result_mr_impl->address);
  if (result_address % ATOMIC_SIZE != 0 || result_address < mr_address ||
      result_mr_impl->size < ATOMIC_SIZE) {
    return false;
  }
  return result_address - mr_address <= result_mr_impl->size - ATOMIC_SIZE;
}

inline error_t endpoint_impl_t::post_sends(int rank, void* buffer, size_t size,
                                           net_imm_data_t imm_data,
                                           void* user_context, bool allow_retry,
                                           bool force_post)
{
  // allow_retry is used by upper layer to decide whether to push the
  // operation to backlog queue if the operation fails
  // if allow_retry is false, the operation will be pushed to backlog queue
  // if it fails
  // force_post is used by backlog queue to force post the operations
  // in the backlog queue
  // We consider the operation high priority if it is either allow_retry is
  // false or force_post is true, to minimize the chance of being pushed back to
  // backlog queue.
  error_t error;
  if (!force_post && !backlog_queue.is_empty(rank)) {
    error = errorcode_t::retry_backlog;
  } else {
    bool high_priority = !allow_retry || force_post;
    error = post_sends_impl(rank, buffer, size, imm_data, user_context,
                            high_priority);
  }
  if (error.is_retry()) {
    if (error.errorcode == errorcode_t::retry_lock) {
      LCI_PCOUNTER_ADD(net_send_post_retry_lock, 1);
    } else if (error.errorcode == errorcode_t::retry_nomem) {
      LCI_PCOUNTER_ADD(net_send_post_retry_nomem, 1);
    } else {
      LCI_PCOUNTER_ADD(net_send_post_retry, 1);
    }
    if (!allow_retry) {
      backlog_queue.push_sends(this, rank, buffer, size, imm_data,
                               user_context);
      error = errorcode_t::done_backlog;
    }
  } else {
    LCI_PCOUNTER_ADD(net_send_post, 1);
  }
  LCI_DBG_Log(LOG_TRACE, "network",
              "post_sends rank %d buffer %p size %lu imm_data %x allow_retry "
              "%d user_context %p force_post %d return %s\n",
              rank, buffer, size, imm_data, allow_retry, user_context,
              force_post, error.get_str());
  return error;
}

inline error_t endpoint_impl_t::post_send(int rank, void* buffer, size_t size,
                                          mr_t mr, net_imm_data_t imm_data,
                                          void* user_context, bool allow_retry,
                                          bool force_post)
{
  error_t error;
  if (!force_post && !backlog_queue.is_empty(rank)) {
    error = errorcode_t::retry_backlog;
  } else {
    bool high_priority = !allow_retry || force_post;
    error = post_send_impl(rank, buffer, size, mr, imm_data, user_context,
                           high_priority);
  }
  if (error.is_retry()) {
    if (error.errorcode == errorcode_t::retry_lock) {
      LCI_PCOUNTER_ADD(net_send_post_retry_lock, 1);
    } else if (error.errorcode == errorcode_t::retry_nomem) {
      LCI_PCOUNTER_ADD(net_send_post_retry_nomem, 1);
    } else {
      LCI_PCOUNTER_ADD(net_send_post_retry, 1);
    }
    if (!allow_retry) {
      backlog_queue.push_send(this, rank, buffer, size, mr, imm_data,
                              user_context);
      error = errorcode_t::posted_backlog;
    }
  } else {
    LCI_PCOUNTER_ADD(net_send_post, 1);
  }
  LCI_DBG_Log(LOG_TRACE, "network",
              "post_send rank %d buffer %p size %lu mr %p imm_data %x "
              "user_context %p allow_retry %d force_post %d return %s\n",
              rank, buffer, size, mr.get_impl(), imm_data, user_context,
              allow_retry, force_post, error.get_str());
  return error;
}

inline error_t endpoint_impl_t::post_puts(int rank, void* buffer, size_t size,
                                          uint64_t offset, rmr_t rmr,
                                          void* user_context, bool allow_retry,
                                          bool force_post)
{
  error_t error;
  if (!force_post && !backlog_queue.is_empty(rank)) {
    error = errorcode_t::retry_backlog;
  } else {
    bool high_priority = !allow_retry || force_post;
    error = post_puts_impl(rank, buffer, size, offset, rmr, user_context,
                           high_priority);
  }
  if (error.is_retry()) {
    LCI_PCOUNTER_ADD(net_write_post_retry, 1);
    if (!allow_retry) {
      backlog_queue.push_puts(this, rank, buffer, size, offset, rmr,
                              user_context);
      error = errorcode_t::done_backlog;
    }
  } else {
    LCI_PCOUNTER_ADD(net_write_post, 1);
    LCI_PCOUNTER_ADD(net_write_writeImm_comp, 1);
  }
  LCI_DBG_Log(LOG_TRACE, "network",
              "post_puts rank %d buffer %p size %lu offset %lu rmr "
              "%p user_context %p allow_retry %d force_post %d return %s\n",
              rank, buffer, size, offset, rmr.base, user_context, allow_retry,
              force_post, error.get_str());
  return error;
}

inline error_t endpoint_impl_t::post_put(int rank, void* buffer, size_t size,
                                         mr_t mr, uint64_t offset, rmr_t rmr,
                                         void* user_context, bool allow_retry,
                                         bool force_post)
{
  error_t error;
  if (!force_post && !backlog_queue.is_empty(rank)) {
    error = errorcode_t::retry_backlog;
  } else {
    bool high_priority = !allow_retry || force_post;
    error = post_put_impl(rank, buffer, size, mr, offset, rmr, user_context,
                          high_priority);
  }
  if (error.is_retry()) {
    LCI_PCOUNTER_ADD(net_write_post_retry, 1);
    if (!allow_retry) {
      backlog_queue.push_put(this, rank, buffer, size, mr, offset, rmr,
                             user_context);
      error = errorcode_t::posted_backlog;
    }
  } else {
    LCI_PCOUNTER_ADD(net_write_post, 1);
  }
  LCI_DBG_Log(LOG_TRACE, "network",
              "post_put rank %d buffer %p size %lu mr %p offset %lu rmr %p "
              "user_context %p allow_retry %d force_post %d return %s\n",
              rank, buffer, size, mr.get_impl(), offset, rmr.base, user_context,
              allow_retry, force_post, error.get_str());
  return error;
}

inline error_t endpoint_impl_t::post_putImms_fallback(
    int rank, void* buffer, size_t size, uint64_t offset, rmr_t rmr,
    net_imm_data_t imm_data, void* user_context, bool high_priority)
{
  // fallback to post_put
  LCI_DBG_Log(
      LOG_TRACE, "network",
      "fallback to post_puts imm_data %x user_context %p (ignored for sends)\n",
      imm_data, user_context);
  error_t error = post_puts_impl(rank, buffer, size, offset, rmr, user_context,
                                 high_priority);
  if (!error.is_retry()) {
    LCI_Assert(error.is_done(), "Unexpected error %s\n", error.get_str());
    // we do not allow retry for post_sends
    // otherwise the above puts might be posted again
    // XXX: but even if puts is posted again, it is not a error
    error_t error = post_sends(rank, nullptr, 0, imm_data, nullptr,
                               false /* allow_retry */);
    LCI_Assert(error.is_done(), "Unexpected error %s\n", error.get_str());
  }
  return error;
}

inline error_t endpoint_impl_t::post_putImms(int rank, void* buffer,
                                             size_t size, uint64_t offset,
                                             rmr_t rmr, net_imm_data_t imm_data,
                                             void* user_context,
                                             bool allow_retry, bool force_post)
{
  error_t error;
  if (!force_post && !backlog_queue.is_empty(rank)) {
    error = errorcode_t::retry_backlog;
  } else {
    bool high_priority = !allow_retry || force_post;
    if (net_context_attr.support_putimm) {
      error = post_putImms_impl(rank, buffer, size, offset, rmr, imm_data,
                                user_context, high_priority);
    } else {
      error = post_putImms_fallback(rank, buffer, size, offset, rmr, imm_data,
                                    user_context, high_priority);
    }
  }
  if (error.is_retry()) {
    LCI_PCOUNTER_ADD(net_writeImm_post_retry, 1);
    if (!allow_retry) {
      backlog_queue.push_putImms(this, rank, buffer, size, offset, rmr,
                                 imm_data, user_context);
      error = errorcode_t::done_backlog;
    }
  } else {
    LCI_PCOUNTER_ADD(net_writeImm_post, 1);
    LCI_PCOUNTER_ADD(net_write_writeImm_comp, 1);
  }
  LCI_DBG_Log(LOG_TRACE, "network",
              "post_putImms rank %d buffer %p size %lu offset %lu "
              "rmr %p imm_data %x user_context %p allow_retry %d force_post %d "
              "return %s\n",
              rank, buffer, size, offset, rmr.base, imm_data, user_context,
              allow_retry, force_post, error.get_str());
  return error;
}

inline error_t endpoint_impl_t::post_putImm_fallback(int rank, void* buffer,
                                                     size_t size, mr_t mr,
                                                     uint64_t offset, rmr_t rmr,
                                                     net_imm_data_t imm_data,
                                                     void* user_context)
{
  // fallback to post_put
  LCI_DBG_Log(LOG_TRACE, "network",
              "fallback to post_put imm_data %x user_context %p\n", imm_data,
              user_context);
  internal_context_extended_t* ectx = new internal_context_extended_t;
  ectx->imm_data_rank = rank;
  ectx->imm_data = imm_data;
  ectx->signal_count = 1;
  ectx->internal_ctx = static_cast<internal_context_t*>(user_context);
  error_t error = post_put_impl(rank, buffer, size, mr, offset, rmr, ectx,
                                true /* high_priority */);
  if (error.is_retry()) {
    delete ectx;
  }
  return error;
}

inline error_t endpoint_impl_t::post_putImm(int rank, void* buffer, size_t size,
                                            mr_t mr, uint64_t offset, rmr_t rmr,
                                            net_imm_data_t imm_data,
                                            void* user_context,
                                            bool allow_retry, bool force_post)
{
  error_t error;
  if (!force_post && !backlog_queue.is_empty(rank)) {
    error = errorcode_t::retry_backlog;
  } else {
    bool high_priority = !allow_retry || force_post;
    if (net_context_attr.support_putimm) {
      error = post_putImm_impl(rank, buffer, size, mr, offset, rmr, imm_data,
                               user_context, high_priority);
    } else {
      error = post_putImm_fallback(rank, buffer, size, mr, offset, rmr,
                                   imm_data, user_context);
    }
  }
  if (error.is_retry()) {
    LCI_PCOUNTER_ADD(net_writeImm_post_retry, 1);
    if (!allow_retry) {
      backlog_queue.push_putImm(this, rank, buffer, size, mr, offset, rmr,
                                imm_data, user_context);
      error = errorcode_t::posted_backlog;
    }
  } else {
    LCI_PCOUNTER_ADD(net_writeImm_post, 1);
  }
  LCI_DBG_Log(LOG_TRACE, "network",
              "post_putImm rank %d buffer %p size %lu mr %p offset "
              "%lu rmr %p imm_data %x user_context %p allow_retry %d "
              "force_post %d return %s\n",
              rank, buffer, size, mr.get_impl(), offset, rmr.base, imm_data,
              user_context, allow_retry, force_post, error.get_str());
  return error;
}

inline error_t endpoint_impl_t::post_get(int rank, void* buffer, size_t size,
                                         mr_t mr, uint64_t offset, rmr_t rmr,
                                         void* user_context, bool allow_retry,
                                         bool force_post)
{
  error_t error;
  if (!force_post && !backlog_queue.is_empty(rank)) {
    error = errorcode_t::retry_backlog;
  } else {
    bool high_priority = !allow_retry || force_post;
    error = post_get_impl(rank, buffer, size, mr, offset, rmr, user_context,
                          high_priority);
  }
  if (error.is_retry()) {
    LCI_PCOUNTER_ADD(net_read_post_retry, 1);
    if (!allow_retry) {
      backlog_queue.push_get(this, rank, buffer, size, mr, offset, rmr,
                             user_context);
      error = errorcode_t::posted_backlog;
    }
  } else {
    LCI_PCOUNTER_ADD(net_read_post, 1);
  }
  LCI_DBG_Log(LOG_TRACE, "network",
              "post_get rank %d buffer %p size %lu mr %p offset %lu rmr %p "
              "user_context %p allow_retry %d force_post %d return %s\n",
              rank, buffer, size, mr.get_impl(), offset, rmr.base, user_context,
              allow_retry, force_post, error.get_str());
  return error;
}

inline error_t endpoint_impl_t::post_fetch_add(
    int rank, uint64_t* result, mr_t result_mr, uint64_t value, uint64_t offset,
    rmr_t rmr, void* user_context, bool allow_retry, bool force_post)
{
  if (!net_context_attr.support_uint64_fetch_add) {
    LCI_Warn(
        "uint64 network fetch-add is unsupported by the selected backend\n");
    return errorcode_t::fatal;
  }
  if (!is_valid_uint64_atomic_result(result, result_mr, device)) {
    LCI_Warn(
        "uint64 network fetch-add requires an aligned 8-byte result within "
        "a memory region registered on the endpoint device\n");
    return errorcode_t::fatal;
  }
  if (!is_valid_uint64_atomic_remote_address(offset, rmr)) {
    LCI_Warn(
        "uint64 network fetch-add requires an aligned 8-byte remote "
        "address\n");
    return errorcode_t::fatal;
  }

  error_t error;
  if (!force_post && !backlog_queue.is_empty(rank)) {
    error = errorcode_t::retry_backlog;
  } else {
    bool high_priority = !allow_retry || force_post;
    // Account before handing the WQE to a backend because another thread may
    // poll and complete it before this function returns.
    add_backend_pending_ops();
    error = post_fetch_add_impl(rank, result, result_mr, value, offset, rmr,
                                user_context, high_priority);
    if (!error.is_posted()) {
      sub_backend_pending_ops();
    }
  }
  if (error.is_retry()) {
    LCI_PCOUNTER_ADD(net_atomic_post_retry, 1);
    if (!allow_retry) {
      backlog_queue.push_fetch_add(this, rank, result, result_mr, value, offset,
                                   rmr, user_context);
      error = errorcode_t::posted_backlog;
    }
  } else if (error.is_posted()) {
    LCI_PCOUNTER_ADD(net_atomic_post, 1);
  }
  LCI_DBG_Log(
      LOG_TRACE, "network",
      "post_fetch_add rank %d result %p result_mr %p value %lu offset %lu "
      "rmr %p user_context %p allow_retry %d force_post %d return %s\n",
      rank, result, result_mr.p_impl, value, offset,
      reinterpret_cast<void*>(rmr.base), user_context, allow_retry, force_post,
      error.get_str());
  return error;
}

inline error_t endpoint_impl_t::post_add(int rank, uint64_t value,
                                         uint64_t offset, rmr_t rmr,
                                         void* user_context, bool allow_retry,
                                         bool force_post)
{
  if (!net_context_attr.support_uint64_fetch_add) {
    LCI_Warn("uint64 network add is unsupported by the selected backend\n");
    return errorcode_t::fatal;
  }
  if (!is_valid_uint64_atomic_remote_address(offset, rmr)) {
    LCI_Warn("uint64 network add requires an aligned 8-byte remote address\n");
    return errorcode_t::fatal;
  }

  error_t error;
  if (!force_post && !backlog_queue.is_empty(rank)) {
    error = errorcode_t::retry_backlog;
  } else {
    bool high_priority = !allow_retry || force_post;
    // See post_fetch_add() for why this is incremented before the backend
    // posts the work request.
    add_backend_pending_ops();
    error =
        post_add_impl(rank, value, offset, rmr, user_context, high_priority);
    if (!error.is_posted()) {
      sub_backend_pending_ops();
    }
  }
  if (error.is_retry()) {
    LCI_PCOUNTER_ADD(net_atomic_post_retry, 1);
    if (!allow_retry) {
      backlog_queue.push_add(this, rank, value, offset, rmr, user_context);
      error = errorcode_t::posted_backlog;
    }
  } else if (error.is_posted()) {
    LCI_PCOUNTER_ADD(net_atomic_post, 1);
  }
  LCI_DBG_Log(LOG_TRACE, "network",
              "post_add rank %d value %lu offset %lu rmr %p user_context %p "
              "allow_retry %d force_post %d return %s\n",
              rank, value, offset, reinterpret_cast<void*>(rmr.base),
              user_context, allow_retry, force_post, error.get_str());
  return error;
}

}  // namespace lci

#endif  // LCI_ENDPOINT_INLINE_HPP
