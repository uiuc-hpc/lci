// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#ifndef LCI_CORE_PACKET_PRE_HPP
#define LCI_CORE_PACKET_PRE_HPP

namespace lci
{
const int POOLID_LOCAL = -1;

struct packet_local_context_t {
  packet_pool_impl_t*
      packet_pool_impl; /* the packet pool this packet belongs to */
  int local_id : 31;    /* id of the local pool to return this packet */
  bool isInPool : 1;    /* Debug use only. Whether this packet is in the packet
                           pool */
  // context of the ongoing communication
  bool is_eager : 1; /* whether this packet is used for eager protocol */
  int rank;
  tag_t tag;
  data_t data;
};

struct packet_t {
  packet_local_context_t local_context;

  void* get_payload_address()
  {
    return reinterpret_cast<char*>(this) + sizeof(packet_local_context_t);
  }
  void put_back();

  mr_t get_mr(device_t device);
  mr_t get_mr(endpoint_t endpoint);
};

static inline packet_t* address2packet(void* address)
{
  return (packet_t*)((char*)address - sizeof(packet_local_context_t));
}

inline void free_ctx_and_signal_comp(internal_context_t* internal_ctx);

}  // namespace lci

#endif  // LCI_CORE_PACKET_PRE_HPP