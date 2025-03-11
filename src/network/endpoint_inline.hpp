// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#ifndef LCI_ENDPOINT_INLINE_HPP
#define LCI_ENDPOINT_INLINE_HPP

namespace lci
{
inline error_t endpoint_impl_t::post_sends(int rank, void* buffer, size_t size,
                                           net_imm_data_t imm_data,
                                           bool allow_retry, bool force_post)
{
  error_t error;
  if (!force_post && !backlog_queue.is_empty()) {
    error = errorcode_t::retry_backlog;
  } else {
    error = post_sends_impl(rank, buffer, size, imm_data);
  }
  if (error.is_retry()) {
    LCI_PCOUNTER_ADD(net_send_post_retry, 1);
    if (!allow_retry) {
      backlog_queue.push_sends(this, rank, buffer, size, imm_data);
      error = errorcode_t::ok_backlog;
    }
  } else {
    LCI_PCOUNTER_ADD(net_send_post, 1);
  }
  return error;
}

inline error_t endpoint_impl_t::post_send(int rank, void* buffer, size_t size,
                                          mr_t mr, net_imm_data_t imm_data,
                                          void* user_context, bool allow_retry,
                                          bool force_post)
{
  error_t error;
  if (!force_post && !backlog_queue.is_empty()) {
    error = errorcode_t::retry_backlog;
  } else {
    error = post_send_impl(rank, buffer, size, mr, imm_data, user_context);
  }
  if (error.is_retry()) {
    LCI_PCOUNTER_ADD(net_send_post_retry, 1);
    if (!allow_retry) {
      backlog_queue.push_send(this, rank, buffer, size, mr, imm_data,
                              user_context);
      error = errorcode_t::posted_backlog;
    }
  } else {
    LCI_PCOUNTER_ADD(net_send_post, 1);
  }
  return error;
}

inline error_t endpoint_impl_t::post_puts(int rank, void* buffer, size_t size,
                                          uintptr_t base, uint64_t offset,
                                          rkey_t rkey, bool allow_retry,
                                          bool force_post)
{
  error_t error;
  if (!force_post && !backlog_queue.is_empty()) {
    error = errorcode_t::retry_backlog;
  } else {
    error = post_puts_impl(rank, buffer, size, base, offset, rkey);
  }
  if (error.is_retry()) {
    LCI_PCOUNTER_ADD(net_send_post_retry, 1);
    if (!allow_retry) {
      backlog_queue.push_puts(this, rank, buffer, size, base, offset, rkey);
      error = errorcode_t::ok_backlog;
    }
  } else {
    LCI_PCOUNTER_ADD(net_send_post, 1);
  }
  return error;
}

inline error_t endpoint_impl_t::post_put(int rank, void* buffer, size_t size,
                                         mr_t mr, uintptr_t base,
                                         uint64_t offset, rkey_t rkey,
                                         void* user_context, bool allow_retry,
                                         bool force_post)
{
  error_t error;
  if (!force_post && !backlog_queue.is_empty()) {
    error = errorcode_t::retry_backlog;
  } else {
    error =
        post_put_impl(rank, buffer, size, mr, base, offset, rkey, user_context);
  }
  if (error.is_retry()) {
    LCI_PCOUNTER_ADD(net_send_post_retry, 1);
    if (!allow_retry) {
      backlog_queue.push_put(this, rank, buffer, size, mr, base, offset, rkey,
                             user_context);
      error = errorcode_t::posted_backlog;
    }
  } else {
    LCI_PCOUNTER_ADD(net_send_post, 1);
  }
  return error;
}

inline error_t endpoint_impl_t::post_putImms(int rank, void* buffer,
                                             size_t size, uintptr_t base,
                                             uint64_t offset, rkey_t rkey,
                                             net_imm_data_t imm_data,
                                             bool allow_retry, bool force_post)
{
  error_t error;
  if (!force_post && !backlog_queue.is_empty()) {
    error = errorcode_t::retry_backlog;
  } else {
    error = post_putImms_impl(rank, buffer, size, base, offset, rkey, imm_data);
  }
  if (error.is_retry()) {
    LCI_PCOUNTER_ADD(net_send_post_retry, 1);
    if (!allow_retry) {
      backlog_queue.push_putImms(this, rank, buffer, size, base, offset, rkey,
                                 imm_data);
      error = errorcode_t::ok_backlog;
    }
  } else {
    LCI_PCOUNTER_ADD(net_send_post, 1);
  }
  return error;
}

inline error_t endpoint_impl_t::post_putImm(int rank, void* buffer, size_t size,
                                            mr_t mr, uintptr_t base,
                                            uint64_t offset, rkey_t rkey,
                                            net_imm_data_t imm_data,
                                            void* user_context,
                                            bool allow_retry, bool force_post)
{
  error_t error;
  if (!force_post && !backlog_queue.is_empty()) {
    error = errorcode_t::retry_backlog;
  } else {
    error = post_putImm_impl(rank, buffer, size, mr, base, offset, rkey,
                             imm_data, user_context);
  }
  if (error.is_retry()) {
    LCI_PCOUNTER_ADD(net_send_post_retry, 1);
    if (!allow_retry) {
      backlog_queue.push_putImm(this, rank, buffer, size, mr, base, offset,
                                rkey, imm_data, user_context);
      error = errorcode_t::posted_backlog;
    }
  } else {
    LCI_PCOUNTER_ADD(net_send_post, 1);
  }
  return error;
}

inline error_t endpoint_impl_t::post_get(int rank, void* buffer, size_t size,
                                         mr_t mr, uintptr_t base,
                                         uint64_t offset, rkey_t rkey,
                                         void* user_context, bool allow_retry,
                                         bool force_post)
{
  error_t error;
  if (!force_post && !backlog_queue.is_empty()) {
    error = errorcode_t::retry_backlog;
  } else {
    error =
        post_get_impl(rank, buffer, size, mr, base, offset, rkey, user_context);
  }
  if (error.is_retry()) {
    LCI_PCOUNTER_ADD(net_send_post_retry, 1);
    if (!allow_retry) {
      backlog_queue.push_get(this, rank, buffer, size, mr, base, offset, rkey,
                             user_context);
      error = errorcode_t::posted_backlog;
    }
  } else {
    LCI_PCOUNTER_ADD(net_send_post, 1);
  }
  return error;
}

}  // namespace lci

#endif  // LCI_ENDPOINT_INLINE_HPP