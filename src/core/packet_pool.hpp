#ifndef LCIXX_CORE_PACKET_POOL_HPP
#define LCIXX_CORE_PACKET_POOL_HPP

namespace lcixx
{
class packet_pool_impl_t
{
 public:
  using attr_t = packet_pool_attr_t;
  attr_t attr;
  packet_pool_impl_t(const attr_t& attr);
  ~packet_pool_impl_t();
  mr_t register_packets(net_device_t net_device);
  void* get() { return pool.get(); }
  void put(void* packet_address)
  {
    LCIXX_Assert(is_packet(packet_address, true),
                 "Not a packet (address %p)!\n", packet_address);
    packet_t* packet = static_cast<packet_t*>(packet_address);
    LCIXX_Assert(!packet->local_context.isInPool,
                 "This packet has already been freed!\n");
    packet->local_context.isInPool = true;
    pool.put(packet, packet->local_context.local_id);
  }

  bool is_packet(void* address, bool include_lcontext = false)
  {
    void* packet_address;
    if (!include_lcontext) {
      packet_address = (packet_t*)((char*)address - offsetof(packet_t, fast));
    } else {
      packet_address = address;
    }
    uintptr_t offset = (uintptr_t)packet_address - (uintptr_t)base_packet_p;
    return (uintptr_t)packet_address >= (uintptr_t)base_packet_p &&
           offset % attr.packet_size == 0 &&
           offset / attr.packet_size < attr.npackets;
  }

  mr_t get_or_register_mr(net_device_t net_device)
  {
    mr_t mr;
    void* p = mrs.get(net_device.p_impl->net_device_id);
    if (!p) {
      mr = register_packets(net_device);
    } else {
      mr.p_impl = static_cast<mr_impl_t*>(p);
    }
    return mr;
  }

  mpmc_set_t pool;
  void* heap;
  // std::unique_ptr<char[]> heap;
  void* base_packet_p;
  size_t heap_size;
  mpmc_array_t mrs;
};
}  // namespace lcixx

#endif  // LCIXX_CORE_PACKET_POOL_HPP