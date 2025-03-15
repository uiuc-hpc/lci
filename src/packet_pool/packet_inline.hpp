// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#ifndef LCI_CORE_PACKET_INLINE_HPP
#define LCI_CORE_PACKET_INLINE_HPP

namespace lci
{
inline void packet_t::put_back() { local_context.packet_pool_impl->put(this); }

inline mr_t packet_t::get_mr(device_t device)
{
  return local_context.packet_pool_impl->get_or_register_mr(device);
}

inline mr_t packet_t::get_mr(endpoint_t endpoint)
{
  device_t device = endpoint.p_impl->device;
  return local_context.packet_pool_impl->get_or_register_mr(device);
}

inline void free_ctx_and_signal_comp(internal_context_t* internal_ctx)
{
  if (internal_ctx->mr_on_the_fly) {
    deregister_data(internal_ctx->data);
  }
  if (internal_ctx->packet_to_free) {
    internal_ctx->packet_to_free->put_back();
  }
  if (!internal_ctx->comp.is_empty()) {
    status_t status = internal_ctx->get_status();
    comp_t comp = internal_ctx->comp;
    internal_context_t::free(internal_ctx);
    comp.p_impl->signal(std::move(status));
  } else {
    internal_context_t::free(internal_ctx);
  }
}

// inline void free_pbuffer_x::call_impl(void* address, runtime_t runtime) const
// {
//   packet_t* packet = address2packet(address);
//   packet->put_back();
// }

// inline void* alloc_pbuffer_x::call_impl(runtime_t runtime,
//                                         packet_pool_t packet_pool) const
// {
//   packet_t* packet = static_cast<packet_t*>(packet_pool.p_impl->get());
//   return packet->get_payload_address();
// }

}  // namespace lci

#endif  // LCI_CORE_PACKET_INLINE_HPP