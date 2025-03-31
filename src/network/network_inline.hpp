// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#ifndef LCI_BACKEND_INLINE_HPP
#define LCI_BACKEND_INLINE_HPP

namespace lci
{
inline void mr_impl_t::deregister()
{
  device.get_impl()->deregister_memory(this);
}

inline rkey_t mr_impl_t::get_rkey()
{
  return device.get_impl()->get_rkey(this);
}

/*************************************************************************************
 * Interface implementations
 * **********************************************************************************/

inline mr_t register_memory_x::call_impl(void* address, size_t size, runtime_t,
                                         device_t device) const
{
  mr_t mr = device.p_impl->register_memory(address, size);
  return mr;
}

inline void deregister_memory_x::call_impl(mr_t* mr, runtime_t) const
{
  mr->p_impl->deregister();
  mr->p_impl = nullptr;
}

inline rkey_t get_rkey_x::call_impl(mr_t mr, runtime_t) const
{
  return mr.p_impl->get_rkey();
}

inline size_t net_poll_cq_x::call_impl(size_t max_polls, net_status_t* statuses,
                                       runtime_t, device_t device) const
{
  return device.p_impl->poll_comp(statuses, max_polls);
}

inline error_t net_post_recv_x::call_impl(void* buffer, size_t size, mr_t mr,
                                          runtime_t, device_t device,
                                          void* user_context) const
{
  return device.p_impl->post_recv(buffer, size, mr, user_context);
}

inline error_t net_post_sends_x::call_impl(int rank, void* buffer, size_t size,
                                           runtime_t, device_t,
                                           endpoint_t endpoint,
                                           net_imm_data_t imm_data) const
{
  return endpoint.p_impl->post_sends(rank, buffer, size, imm_data);
}

inline error_t net_post_send_x::call_impl(int rank, void* buffer, size_t size,
                                          mr_t mr, runtime_t, device_t,
                                          endpoint_t endpoint,
                                          net_imm_data_t imm_data,
                                          void* user_context) const
{
  return endpoint.p_impl->post_send(rank, buffer, size, mr, imm_data,
                                    user_context);
}

inline error_t net_post_puts_x::call_impl(int rank, void* buffer, size_t size,
                                          uintptr_t base, uint64_t offset,
                                          rkey_t rkey, runtime_t, device_t,
                                          endpoint_t endpoint) const
{
  return endpoint.p_impl->post_puts(rank, buffer, size, base, offset, rkey);
}

inline error_t net_post_put_x::call_impl(int rank, void* buffer, size_t size,
                                         mr_t mr, uintptr_t base,
                                         uint64_t offset, rkey_t rkey,
                                         runtime_t, device_t,
                                         endpoint_t endpoint,
                                         void* user_context) const
{
  return endpoint.p_impl->post_put(rank, buffer, size, mr, base, offset, rkey,
                                   user_context);
}

inline error_t net_post_putImms_x::call_impl(int rank, void* buffer,
                                             size_t size, uintptr_t base,
                                             uint64_t offset, rkey_t rkey,
                                             runtime_t, device_t,
                                             endpoint_t endpoint,
                                             net_imm_data_t imm_data) const
{
  return endpoint.p_impl->post_putImms(rank, buffer, size, base, offset, rkey,
                                       imm_data);
}

inline error_t net_post_putImm_x::call_impl(
    int rank, void* buffer, size_t size, mr_t mr, uintptr_t base,
    uint64_t offset, rkey_t rkey, runtime_t, device_t, endpoint_t endpoint,
    net_imm_data_t imm_data, void* user_context) const
{
  return endpoint.p_impl->post_putImm(rank, buffer, size, mr, base, offset,
                                      rkey, imm_data, user_context);
}

inline error_t net_post_get_x::call_impl(int rank, void* buffer, size_t size,
                                         mr_t mr, uintptr_t base,
                                         uint64_t offset, rkey_t rkey,
                                         runtime_t, device_t,
                                         endpoint_t endpoint,
                                         void* user_context) const
{
  return endpoint.p_impl->post_get(rank, buffer, size, mr, base, offset, rkey,
                                   user_context);
}

}  // namespace lci

#endif  // LCI_BACKEND_INLINE_HPP