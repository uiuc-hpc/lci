#ifndef PACKET_H_
#define PACKET_H_

struct __attribute__((packed)) LCII_packet_context {
  // Most of the current ctx requires 128-bits (FIXME)
  int64_t hwctx[2];
  // Here is LCI context.
  LCII_pool_t* pkpool; /* the packet pool this packet belongs to */
  int poolid;          /* id of the pool to return this packet.
                        * -1 means returning to the local pool */
  int src_rank;
  int length; /* length for its payload */
};

struct __attribute__((packed)) LCII_packet_rts_t {
  LCI_msg_type_t msg_type; /* type of the long message */
  uintptr_t
      send_ctx; /* the address of the related context on the source side */
  union {
    // for LCI_LONG
    size_t size;
    // for LCI_IOVEC
    struct {
      int count;
      size_t piggy_back_size;
      size_t size_p[0];
    };
  };
};

struct __attribute__((packed)) LCII_packet_rtr_iovec_info_t {
  uint64_t rkey;
  uintptr_t remote_addr_base;
  LCIS_offset_t remote_addr_offset;
};

struct __attribute__((packed)) LCII_packet_rtr_t {
  LCI_msg_type_t msg_type; /* type of the long message */
  uintptr_t
      send_ctx; /* the address of the related context on the source side */
  union {
    // for LCI_LONG
    struct {
      uint64_t rkey;
      uintptr_t remote_addr_base;
      LCIS_offset_t remote_addr_offset;
      uint32_t
          recv_ctx_key; /* the id of the related context on the target side */
    };
    // for LCI_IOVEC
    struct {
      uintptr_t recv_ctx;
      struct LCII_packet_rtr_iovec_info_t remote_iovecs_p[0];
    };
  };
};

struct __attribute__((packed, aligned(LCI_CACHE_LINE))) LCII_packet_data_t {
  union {
    struct LCII_packet_rts_t rts;
    struct LCII_packet_rtr_t rtr;
    char address[0];
  };
};

typedef struct __attribute__((packed)) LCII_packet_t {
  struct LCII_packet_context context;
  struct LCII_packet_data_t data;
} LCII_packet_t;

static inline void LCII_free_packet(LCII_packet_t* packet)
{
  LCM_DBG_Assert(((uint64_t)packet + sizeof(struct LCII_packet_context)) %
                         LCI_CACHE_LINE ==
                     0,
                 "Not a packet (address %p)!\n", packet);
  if (packet->context.poolid != -1)
    LCII_pool_put_to(packet->context.pkpool, packet, packet->context.poolid);
  else
    LCII_pool_put(packet->context.pkpool, packet);
}

static inline LCII_packet_t* LCII_mbuffer2packet(LCI_mbuffer_t mbuffer)
{
  return (LCII_packet_t*)(mbuffer.address - offsetof(LCII_packet_t, data));
}

#endif
