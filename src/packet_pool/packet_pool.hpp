// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#ifndef LCI_CORE_PACKET_POOL_HPP
#define LCI_CORE_PACKET_POOL_HPP

namespace lci
{
class packet_pool_impl_t
{
 public:
  using attr_t = packet_pool_attr_t;

  packet_pool_impl_t(const attr_t& attr);
  ~packet_pool_impl_t();
  // Register all packets to the device
  mr_t register_packets(device_t device);
  // Deregister all packets from the device
  void deregister_packets(device_t device);
  // Get the memory region of all packets
  mr_t get_or_register_mr(device_t device);
  // Get a packet from the pool
  packet_t* get(bool blocking = false);
  // Put a packet back to the pool
  void put(packet_t* p_packet);
  // Check if an address is a packet
  bool is_packet(void* address, bool include_lcontext = false);
  // Get the payload size of a packet
  size_t get_payload_size() const
  {
    return attr.packet_size - sizeof(packet_local_context_t);
  }
  size_t get_size() const { return pool.size(); }
  int get_local_id() const { return pool.get_local_set_id(); }
  // Report lost packets
  void report_lost_packets(int npackets) { npacket_lost += npackets; }

  attr_t attr;

 private:
  mpmc_set_t pool;
  void* heap;
  void* base_packet_p;
  size_t heap_size;
  mpmc_array_t mrs;
  std::atomic<size_t> npacket_lost;
};

inline packet_t* packet_pool_impl_t::get(bool blocking)
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

inline void packet_pool_impl_t::put(packet_t* p_packet)
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

inline bool packet_pool_impl_t::is_packet(void* address, bool include_lcontext)
{
  void* packet_address;
  if (!include_lcontext) {
    packet_address =
        (packet_t*)((char*)address - sizeof(packet_local_context_t));
  } else {
    packet_address = address;
  }
  uintptr_t offset = (uintptr_t)packet_address - (uintptr_t)base_packet_p;
  return (uintptr_t)packet_address >= (uintptr_t)base_packet_p &&
         offset % attr.packet_size == 0 &&
         offset / attr.packet_size < attr.npackets;
}

}  // namespace lci

#endif  // LCI_CORE_PACKET_POOL_HPP