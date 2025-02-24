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
};

struct packet_t {
  packet_local_context_t local_context;
  char payload[0];

  void* get_payload_address() { return payload; }

  void put_back();

  mr_t get_mr(net_device_t net_device);
  mr_t get_mr(net_endpoint_t net_endpoint);
};

static inline packet_t* address2packet(void* address)
{
  return (packet_t*)((char*)address - offsetof(packet_t, payload));
}

}  // namespace lci

#endif  // LCI_CORE_PACKET_PRE_HPP