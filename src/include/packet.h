#ifndef PACKET_H_
#define PACKET_H_

#include "config.h"
#include <stdint.h>

#define USER_MANAGED 99

#define STATIC_ASSERT(COND,MSG) typedef char static_assertion_##MSG[(COND)?1:-1]

#define lc_pk_init(ep_, pid_, proto_, p) \
  p->context.ep = (ep_);                 \
  p->context.poolid = (pid_);            \
  p->context.ref = 1;                    \
  p->context.proto = (proto_);

struct __attribute__((packed, aligned(64))) packet_context {
  // Most of the current ctx requires 128-bits (FIXME)
  int64_t hwctx[2];
  // Here is LLCI context.
  LCI_syncl_t sync_s;
  LCI_syncl_t* sync;
  LCI_endpoint_t ep;
  intptr_t rma_mem;
  int16_t proto;
  lc_pool *pkpool;
  int8_t poolid;      /* id of the pool to return this packet.
                       * -1 means returning to the local pool */
  int length;         /* length for its payload */
  int8_t ref;
};

struct __attribute__((packed)) packet_rts {
  uintptr_t ctx;
  size_t size;
};

struct __attribute__((packed)) packet_rtr {
  uintptr_t ctx;
  size_t size;
  intptr_t tgt_addr;
  uint64_t rkey;
  uint32_t ctx_id;
};

struct __attribute__((packed)) packet_data {
  union {
    struct packet_rts rts;
    struct packet_rtr rtr;
    char address[0];
  };
};

struct __attribute__((aligned(64))) lc_packet {
  union {
    struct packet_context context;
  };
  struct packet_data data;
};

static inline void LCII_free_packet(lc_packet* packet) {
  if (packet->context.poolid != -1)
    lc_pool_put_to(packet->context.pkpool, packet, packet->context.poolid);
  else
    lc_pool_put(packet->context.pkpool, packet);
}

static inline lc_packet* LCII_mbuffer2packet(LCI_mbuffer_t mbuffer) {
  return (lc_packet*) (mbuffer.address - offsetof(lc_packet, data));
}

#endif
