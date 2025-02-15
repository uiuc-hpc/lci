#ifndef LCI_CORE_PACKET_PRE_HPP
#define LCI_CORE_PACKET_PRE_HPP

namespace lci
{
const int POOLID_LOCAL = -1;

struct __attribute__((packed)) packet_local_context_t {
  packet_pool_impl_t*
      packet_pool_impl; /* the packet pool this packet belongs to */
  int local_id;         /* id of the local pool to return this packet */
  bool isInPool; /* Debug use only. Whether this packet is in the packet pool */
};

struct __attribute__((packed)) packet_remote_context_t {
  uint64_t tag;
  uintptr_t rcomp;
};

struct __attribute__((packed)) packet_data_rts_t {
  uintptr_t
      send_ctx; /* the address of the related context on the source side */
  packet_data_rdv_type_t rdv_type; /* type of this rendezvous message */
  union {
    // for a single message
    size_t size;
    // for iovec
    struct {
      int count;
      size_t piggy_back_size;
      size_t size_p[0];
    };
  };
};

struct __attribute__((packed)) packet_data_rtr_rbuffer_info_t {
  rkey_t rkey;
  uintptr_t remote_addr_base;
  uint64_t remote_addr_offset;
};

struct __attribute__((packed)) packet_data_rtr_t {
  uintptr_t
      send_ctx; /* the address of the related context on the source side */
  packet_data_rdv_type_t rdv_type; /* type of this rendezvous message */
  union {
    // When using writeimm protocol
    uint32_t
        recv_ctx_key; /* the id of the related context on the target side */
    // when using write protocol
    uintptr_t recv_ctx;
  };
  struct packet_data_rtr_rbuffer_info_t rbuffer_info_p[0];
};

struct __attribute__((packed)) packet_data_t {
  union {
    packet_data_rts_t rts;
    packet_data_rtr_t rtr;
    char address[0];
  };
};

struct __attribute__((packed)) packet_t {
  packet_local_context_t local_context;
  union alignas(LCI_CACHE_LINE) {
    struct {
      packet_data_t data;
    } fast;
    struct {
      packet_remote_context_t remote_context;
      packet_data_t data;
    } full;
  };

  void* get_message_address() { return &fast; }

  void put_back();

  mr_t get_mr(net_device_t net_device);
};

static inline packet_t* address2packet(void* address)
{
  return (packet_t*)((char*)address - offsetof(packet_t, fast));
}

}  // namespace lci

#endif  // LCI_CORE_PACKET_PRE_HPP