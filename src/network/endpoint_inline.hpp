// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#ifndef LCI_ENDPOINT_INLINE_HPP
#define LCI_ENDPOINT_INLINE_HPP

namespace lci
{
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

}  // namespace lci

#endif  // LCI_ENDPOINT_INLINE_HPP
