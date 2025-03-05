#ifndef LCI_CORE_PACKET_POOL_HPP
#define LCI_CORE_PACKET_POOL_HPP

namespace lci
{
class packet_pool_impl_t
{
 public:
  using attr_t = packet_pool_attr_t;
  attr_t attr;
  packet_pool_impl_t(const attr_t& attr);
  ~packet_pool_impl_t();
  mr_t register_packets(device_t device);
  void deregister_packets(device_t device);
  size_t get_pmessage_size() const
  {
    return attr.packet_size - sizeof(packet_local_context_t);
  }
  packet_t* get(bool blocking = false)
  {
    int nattempts = 1;
    if (blocking) {
      // Should only take a few seconds
      nattempts = 1000000;
    }
    auto* packet = static_cast<packet_t*>(pool.get(nattempts));
    LCI_Assert(packet || !blocking,
               "Failed to get a packet in a blocking get! We are likely run "
               "out of packets\n");
    if (packet) {
      packet->local_context.packet_pool_impl = this;
      packet->local_context.isInPool = false;
      packet->local_context.local_id = mpmc_set_t::LOCAL_SET_ID_NULL;
      LCI_PCOUNTER_ADD(packet_get, 1);
    } else {
      LCI_PCOUNTER_ADD(packet_get_retry, 1);
    }
    return packet;
  }
  void put(packet_t* p_packet)
  {
    LCI_Assert(is_packet(p_packet, true), "Not a packet (address %p)!\n",
               p_packet);
    packet_t* packet = static_cast<packet_t*>(p_packet);
    LCI_Assert(!packet->local_context.isInPool,
               "This packet has already been freed!\n");
    packet->local_context.isInPool = true;
    pool.put(packet, packet->local_context.local_id);
    LCI_PCOUNTER_ADD(packet_put, 1);
  }

  bool is_packet(void* address, bool include_lcontext = false)
  {
    void* packet_address;
    if (!include_lcontext) {
      packet_address =
          (packet_t*)((char*)address - offsetof(packet_t, payload));
    } else {
      packet_address = address;
    }
    uintptr_t offset = (uintptr_t)packet_address - (uintptr_t)base_packet_p;
    return (uintptr_t)packet_address >= (uintptr_t)base_packet_p &&
           offset % attr.packet_size == 0 &&
           offset / attr.packet_size < attr.npackets;
  }

  mr_t get_or_register_mr(device_t device)
  {
    mr_t mr;
    void* p = mrs.get(device.get_impl()->get_id());
    if (!p) {
      mr = register_packets(device);
    } else {
      mr.p_impl = static_cast<mr_impl_t*>(p);
    }
    return mr;
  }

  void report_lost_packets(int npackets) { npacket_lost += npackets; }

  mpmc_set_t pool;
  void* heap;
  // std::unique_ptr<char[]> heap;
  void* base_packet_p;
  size_t heap_size;
  mpmc_array_t mrs;
  std::atomic<int> npacket_lost;
};
}  // namespace lci

#endif  // LCI_CORE_PACKET_POOL_HPP