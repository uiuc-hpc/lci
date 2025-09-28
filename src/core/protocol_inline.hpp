// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#ifndef LCI_CORE_PROTOCOL_INLINE_HPP
#define LCI_CORE_PROTOCOL_INLINE_HPP

namespace lci
{
inline internal_context_t::~internal_context_t()
{
  if (mr_on_the_fly) {
    deregister_memory(&mr);
  }
  if (packet_to_free) {
    packet_to_free->put_back();
  }
  if (is_user_posted_op) {
    endpoint.get_impl()->sub_pending_ops();
  }
}
}  // namespace lci

#endif  // LCI_CORE_PROTOCOL_INLINE_HPP