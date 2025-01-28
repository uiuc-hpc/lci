#ifndef LCIXX_CORE_PACKET_POST_HPP
#define LCIXX_CORE_PACKET_POST_HPP

namespace lcixx
{
inline mr_t packet_t::get_mr(net_device_t net_device)
{
  return local_context.packet_pool_impl->get_or_register_mr(net_device);
}

inline void put_packet_x::call() const
{
  runtime_t runtime = runtime_.get_value_or(g_default_runtime);
  packet_t* packet = address2packet(address_);
  packet->local_context.packet_pool_impl->put(packet);
}

inline void get_packet_x::call() const
{
  runtime_t runtime = runtime_.get_value_or(g_default_runtime);
  packet_pool_t packet_pool =
      packet_pool_.get_value_or(runtime.p_impl->packet_pool);
  packet_t* packet = static_cast<packet_t*>(packet_pool.p_impl->get());
  *address_ = packet->get_message_address();
}

}  // namespace lcixx

#endif  // LCIXX_CORE_PACKET_POST_HPP