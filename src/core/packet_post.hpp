#ifndef LCIXX_CORE_PACKET_POST_HPP
#define LCIXX_CORE_PACKET_POST_HPP

namespace lcixx
{
inline mr_t packet_t::get_mr(net_device_t net_device)
{
  return local_context.packet_pool_impl->get_or_register_mr(net_device);
}
}  // namespace lcixx

#endif  // LCIXX_CORE_PACKET_POST_HPP