#ifndef PACKET_H_
#define PACKET_H_

struct __attribute__((packed)) packet_context {
  // Most of the current ctx requires 128-bits (FIXME)
  int64_t hwctx[2];
  // Here is LCI context.
  lc_pool *pkpool;    /* the packet pool this packet belongs to */
  int8_t poolid;      /* id of the pool to return this packet.
                       * -1 means returning to the local pool */
  int src_rank;
  int length;         /* length for its payload */
};

struct __attribute__((packed)) packet_rts {
  LCI_msg_type_t msg_type;  /* type of the long message */
  uintptr_t send_ctx;  /* the address of the related context on the source side */
  size_t size;
};

struct __attribute__((packed)) packet_rtr {
  LCI_msg_type_t msg_type;  /* type of the long message */
  uintptr_t send_ctx;  /* the address of the related context on the source side */
  intptr_t remote_addr_base;
  uint32_t remote_addr_offset;
  uint64_t rkey;
  uint32_t recv_ctx_key;  /* the id of the related context on the target side */
};

struct __attribute__((packed, aligned(LC_CACHE_LINE))) packet_data {
  union {
    struct packet_rts rts;
    struct packet_rtr rtr;
    char address[0];
  };
};

struct __attribute__((packed)) lc_packet {
  struct packet_context context;
  struct packet_data data;
};

static inline void LCII_free_packet(lc_packet* packet) {
  LCM_DBG_Assert(((uint64_t) packet + sizeof(struct packet_context)) % LC_CACHE_LINE == 0, "Not a packet (address %p)!\n", packet);
  if (packet->context.poolid != -1)
    lc_pool_put_to(packet->context.pkpool, packet, packet->context.poolid);
  else
    lc_pool_put(packet->context.pkpool, packet);
}

static inline lc_packet* LCII_mbuffer2packet(LCI_mbuffer_t mbuffer) {
  return (lc_packet*) (mbuffer.address - offsetof(lc_packet, data));
}

#endif
