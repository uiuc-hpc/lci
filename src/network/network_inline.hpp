// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#ifndef LCI_BACKEND_INLINE_HPP
#define LCI_BACKEND_INLINE_HPP

namespace lci
{
inline void mr_impl_t::deregister()
{
  device.get_impl()->deregister_memory(this);
}

inline rmr_t mr_impl_t::get_rmr()
{
  rmr_t ret;
  ret.base = reinterpret_cast<uintptr_t>(address);
  ret.opaque_rkey = device.get_impl()->get_rkey(this);
  return ret;
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

inline rmr_t get_rmr_x::call_impl(mr_t mr, runtime_t) const
{
  return mr.p_impl->get_rmr();
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
  auto ret = device.p_impl->post_recv(buffer, size, mr, user_context);
  LCI_DBG_Log(LOG_TRACE, "network",
              "post_recv buffer %p size %lu mr %p user_context %p return %s\n",
              buffer, size, mr.get_impl(), user_context, ret.get_str());
  return ret;
}

inline error_t net_post_sends_x::call_impl(int rank, void* buffer, size_t size,
                                           runtime_t, device_t,
                                           endpoint_t endpoint,
                                           net_imm_data_t imm_data,
                                           void* user_context) const
{
  auto ret =
      endpoint.p_impl->post_sends(rank, buffer, size, imm_data, user_context);
  LCI_DBG_Log(LOG_TRACE, "network",
              "post_sends rank %d buffer %p size %lu imm_data %x user_context "
              "%p return %s\n",
              rank, buffer, size, imm_data, user_context, ret.get_str());
  return ret;
}

inline error_t net_post_send_x::call_impl(int rank, void* buffer, size_t size,
                                          mr_t mr, runtime_t, device_t,
                                          endpoint_t endpoint,
                                          net_imm_data_t imm_data,
                                          void* user_context) const
{
  auto ret = endpoint.p_impl->post_send(rank, buffer, size, mr, imm_data,
                                        user_context);
  LCI_DBG_Log(LOG_TRACE, "network",
              "post_send rank %d buffer %p size %lu mr %p imm_data %x "
              "user_context %p return %s\n",
              rank, buffer, size, mr.get_impl(), imm_data, user_context,
              ret.get_str());
  return ret;
}

inline error_t net_post_puts_x::call_impl(int rank, void* buffer, size_t size,
                                          uint64_t offset, rmr_t rmr, runtime_t,
                                          device_t, endpoint_t endpoint,
                                          void* user_context) const
{
  auto ret =
      endpoint.p_impl->post_puts(rank, buffer, size, offset, rmr, user_context);
  LCI_DBG_Log(LOG_TRACE, "network",
              "post_puts rank %d buffer %p size %lu offset %lu rmr %p "
              "user_context %p return %s\n",
              rank, buffer, size, offset, rmr, user_context, ret.get_str());
  return ret;
}

inline error_t net_post_put_x::call_impl(int rank, void* buffer, size_t size,
                                         mr_t mr, uint64_t offset, rmr_t rmr,
                                         runtime_t, device_t,
                                         endpoint_t endpoint,
                                         void* user_context) const
{
  auto ret = endpoint.p_impl->post_put(rank, buffer, size, mr, offset, rmr,
                                       user_context);
  LCI_DBG_Log(LOG_TRACE, "network",
              "post_put rank %d buffer %p size %lu mr %p offset %lu "
              "rmr %p user_context %p return %s\n",
              rank, buffer, size, mr.get_impl(), offset, rmr, user_context,
              ret.get_str());
  return ret;
}

inline error_t net_post_putImms_x::call_impl(int rank, void* buffer,
                                             size_t size, uint64_t offset,
                                             rmr_t rmr, runtime_t, device_t,
                                             endpoint_t endpoint,
                                             net_imm_data_t imm_data,
                                             void* user_context) const
{
  auto ret = endpoint.p_impl->post_putImms(rank, buffer, size, offset, rmr,
                                           imm_data, user_context);
  LCI_DBG_Log(LOG_TRACE, "network",
              "post_putImms rank %d buffer %p size %lu offset %lu rmr %p "
              "imm_data %x user_context %p return %s\n",
              rank, buffer, size, offset, rmr, imm_data, user_context,
              ret.get_str());
  return ret;
}

inline error_t net_post_putImm_x::call_impl(int rank, void* buffer, size_t size,
                                            mr_t mr, uint64_t offset, rmr_t rmr,
                                            runtime_t, device_t,
                                            endpoint_t endpoint,
                                            net_imm_data_t imm_data,
                                            void* user_context) const
{
  auto ret = endpoint.p_impl->post_putImm(rank, buffer, size, mr, offset, rmr,
                                          imm_data, user_context);
  LCI_DBG_Log(LOG_TRACE, "network",
              "post_putImm rank %d buffer %p size %lu mr %p offset "
              "%lu rmr %p imm_data %x user_context %p return %s\n",
              rank, buffer, size, mr.get_impl(), offset, rmr, imm_data,
              user_context, ret.get_str());
  return ret;
}

inline error_t net_post_get_x::call_impl(int rank, void* buffer, size_t size,
                                         mr_t mr, uint64_t offset, rmr_t rmr,
                                         runtime_t, device_t,
                                         endpoint_t endpoint,
                                         void* user_context) const
{
  auto ret = endpoint.p_impl->post_get(rank, buffer, size, mr, offset, rmr,
                                       user_context);
  LCI_DBG_Log(LOG_TRACE, "network",
              "post_get rank %d buffer %p size %lu mr %p offset %lu "
              "rmr %p user_context %p return %s\n",
              rank, buffer, size, mr.get_impl(), offset, rmr, user_context,
              ret.get_str());
  return ret;
}

}  // namespace lci

#endif  // LCI_BACKEND_INLINE_HPP
