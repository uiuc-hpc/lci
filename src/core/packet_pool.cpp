#include "lci_internal.hpp"
#include <memory>

namespace lci
{
packet_pool_impl_t::packet_pool_impl_t(const attr_t& attr_)
    : attr(attr_),
      pool(),
      mrs(64),
      heap(nullptr),
      base_packet_p(nullptr),
      heap_size(0),
      npacket_lost(0)
{
  if (attr.npackets > 0) {
    heap_size = attr.npackets * attr.packet_size + LCI_CACHE_LINE;
    heap = alloc_memalign(get_page_size(), heap_size);
    base_packet_p =
        (char*)heap + LCI_CACHE_LINE - sizeof(packet_local_context_t);
    for (size_t i = 0; i < attr.npackets; i++) {
      packet_t* packet =
          (packet_t*)((char*)base_packet_p + i * attr.packet_size);
      LCI_Assert(
          ((uint64_t)packet->get_message_address()) % LCI_CACHE_LINE == 0,
          "packet.data is not well-aligned %p\n",
          packet->get_message_address());
      LCI_Assert(is_packet(packet->get_message_address()),
                 "Not a packet. The computation is wrong!\n");
      packet->local_context.packet_pool_impl = this;
      packet->local_context.local_id = pool.get_local_set_id();
      packet->local_context.isInPool = true;
      pool.put(packet);
    }
  }
}

packet_pool_impl_t::~packet_pool_impl_t()
{
  // check whether there are any packets missing
  if (attr.npackets > 0) {
    int total = pool.size() + npacket_lost;
    if (total != attr.npackets) {
      LCI_Warn("Lost %d packets\n", attr.npackets - total);
    }
  }
  for (int i = 0; i < mrs.get_size(); i++) {
    mr_impl_t* p_mr = static_cast<mr_impl_t*>(mrs.get(i));
    if (p_mr) {
      mr_t mr;
      mr.p_impl = p_mr;
      deregister_memory_x(&mr).call();
    }
  }
  free(heap);
}

mr_t packet_pool_impl_t::register_packets(net_device_t net_device)
{
  mr_t mr;
  if (heap) {
    mr = register_memory_x(heap, heap_size).net_device(net_device)();
    mrs.put(net_device.p_impl->net_device_id, mr.p_impl);
  }
  return mr;
}

void packet_pool_impl_t::deregister_packets(net_device_t net_device)
{
  mr_impl_t* p_mr =
      static_cast<mr_impl_t*>(mrs.get(net_device.p_impl->net_device_id));
  if (p_mr) {
    mr_t mr;
    mr.p_impl = p_mr;
    deregister_memory_x(&mr).call();
    mrs.put(net_device.p_impl->net_device_id, nullptr);
  }
}

packet_pool_t alloc_packet_pool_x::call_impl(runtime_t runtime,
                                             size_t packet_size,
                                             size_t npackets) const
{
  packet_pool_t packet_pool;
  packet_pool.p_impl = new packet_pool_impl_t({packet_size, npackets});
  return packet_pool;
}

void free_packet_pool_x::call_impl(packet_pool_t* packet_pool,
                                   runtime_t runtime) const
{
  delete packet_pool->p_impl;
  packet_pool->p_impl = nullptr;
}

void bind_packet_pool_x::call_impl(net_device_t net_device,
                                   packet_pool_t packet_pool,
                                   runtime_t runtime) const
{
  net_device.p_impl->bind_packet_pool(packet_pool);
}

void unbind_packet_pool_x::call_impl(net_device_t net_device,
                                     runtime_t runtime) const
{
  net_device.p_impl->unbind_packet_pool();
}

}  // namespace lci