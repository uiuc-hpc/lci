#ifndef LCI_CORE_PACKET_POST_HPP
#define LCI_CORE_PACKET_POST_HPP

namespace lci
{
inline void packet_t::put_back() { local_context.packet_pool_impl->put(this); }

inline mr_t packet_t::get_mr(net_device_t net_device)
{
  return local_context.packet_pool_impl->get_or_register_mr(net_device);
}

inline void free_pbuffer_x::call_impl(void* address, runtime_t runtime) const
{
  packet_t* packet = address2packet(address);
  packet->put_back();
}

inline void* alloc_pbuffer_x::call_impl(runtime_t runtime,
                                        packet_pool_t packet_pool) const
{
  packet_t* packet = static_cast<packet_t*>(packet_pool.p_impl->get());
  return packet->get_message_address();
}

}  // namespace lci

#endif  // LCI_CORE_PACKET_POST_HPP