// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include "lci_internal.hpp"
#include <memory>

namespace lci
{
packet_pool_impl_t::packet_pool_impl_t(const attr_t& attr_)
    : attr(attr_),
      pool(),
      heap(nullptr),
      base_packet_p(nullptr),
      heap_size(0),
      mrs(64),
      npacket_lost(0)
{
  if (attr.npackets > 0) {
    heap_size = attr.npackets * attr.packet_size + LCI_CACHE_LINE;
    heap = alloc_memalign(heap_size, get_page_size());
    base_packet_p =
        (char*)heap + LCI_CACHE_LINE - sizeof(packet_local_context_t);
    for (size_t i = 0; i < attr.npackets; i++) {
      packet_t* packet =
          (packet_t*)((char*)base_packet_p + i * attr.packet_size);
      LCI_DBG_Assert(
          ((uint64_t)packet->get_payload_address()) % LCI_CACHE_LINE == 0,
          "packet.data is not well-aligned %p\n",
          packet->get_payload_address());
      LCI_DBG_Assert(is_packet(packet->get_payload_address()),
                     "Not a packet. The computation is wrong!\n");
      pool.put(packet);
    }
  }
}

packet_pool_impl_t::~packet_pool_impl_t()
{
  // check whether there are any packets missing
  if (attr.npackets > 0) {
    size_t total = pool.size() + npacket_lost;
    if (total != attr.npackets) {
      LCI_Warn("Lost %d packets\n", attr.npackets - total);
    }
  }
  for (size_t i = 0; i < mrs.get_size(); i++) {
    mr_impl_t* p_mr = static_cast<mr_impl_t*>(mrs.get(i));
    if (p_mr) {
      mr_t mr;
      mr.p_impl = p_mr;
      deregister_memory_x(&mr).call();
    }
  }
  free(heap);
}

mr_t packet_pool_impl_t::register_packets(device_t device)
{
  mr_t mr;
  if (heap) {
    mr = register_memory_x(heap, heap_size).device(device)();
    mrs.put(device.get_attr_uid(), mr.p_impl);
  }
  return mr;
}

void packet_pool_impl_t::deregister_packets(device_t device)
{
  mr_impl_t* p_mr = static_cast<mr_impl_t*>(mrs.get(device.get_attr_uid()));
  if (p_mr) {
    mr_t mr;
    mr.p_impl = p_mr;
    deregister_memory_x(&mr).call();
    mrs.put(device.get_attr_uid(), nullptr);
  }
}

mr_t packet_pool_impl_t::get_or_register_mr(device_t device)
{
  mr_t mr;
  void* p = mrs.get(device.get_attr_uid());
  if (!p) {
    mr = register_packets(device);
  } else {
    mr.p_impl = static_cast<mr_impl_t*>(p);
  }
  return mr;
}

packet_pool_t alloc_packet_pool_x::call_impl(runtime_t, size_t packet_size,
                                             size_t npackets,
                                             void* user_context) const
{
  packet_pool_attr_t attr;
  attr.packet_size = packet_size;
  attr.npackets = npackets;
  attr.user_context = user_context;
  packet_pool_t packet_pool;
  packet_pool.p_impl = new packet_pool_impl_t(attr);
  return packet_pool;
}

void free_packet_pool_x::call_impl(packet_pool_t* packet_pool, runtime_t) const
{
  delete packet_pool->p_impl;
  packet_pool->p_impl = nullptr;
}

void register_packet_pool_x::call_impl(packet_pool_t packet_pool,
                                       device_t device, runtime_t) const
{
  packet_pool.p_impl->register_packets(device);
}

void deregister_packet_pool_x::call_impl(packet_pool_t packet_pool,
                                         device_t device, runtime_t) const
{
  packet_pool.p_impl->deregister_packets(device);
}

void* get_upacket_x::call_impl(runtime_t, packet_pool_t packet_pool) const
{
  packet_t* packet = static_cast<packet_t*>(packet_pool.p_impl->get());
  if (!packet) {
    return nullptr;
  }
  return packet->get_payload_address();
}

void put_upacket_x::call_impl(void* upacket, runtime_t) const
{
  packet_t* packet = address2packet(upacket);
  packet->put_back();
}

}  // namespace lci