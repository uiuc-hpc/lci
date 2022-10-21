#ifndef PACKET_H_
#define PACKET_H_

#include "config.h"
#include <stdint.h>
#include "lc.h"

#define PACKET_DATA_SIZE (LC_PACKET_SIZE - sizeof(struct packet_context))

#define lci_pk_init(ep_, pid_, proto_, p)  \
  p->context.ep = (ep_);                   \
  p->context.poolid = (pid_);              \
  p->context.ref = 1;                      \
  p->context.proto = (proto_);

struct __attribute__((packed)) packet_context {
  // Most of the current ctx requires 128-bits (FIXME)
  uint64_t hwctx[2];
  // Here is LLCI context.
  struct lc_req req_s;
  struct lc_req* req;
  struct lci_ep* ep;
  uint64_t rma_mem;
  int16_t proto;
  int8_t poolid;
  int8_t ref;
};

struct __attribute__((packed)) packet_rts {
  lc_send_cb cb;
  uintptr_t ce;
  uintptr_t src_addr;
  size_t size;
  uintptr_t tgt_addr;
};

struct __attribute__((packed)) packet_rtr {
  lc_send_cb cb;
  uintptr_t ce;
  uintptr_t src_addr;
  size_t size;
  uintptr_t tgt_addr;
  uint32_t rkey;
  uint32_t comm_id;
};

struct __attribute__((packed)) packet_data {
  union {
    struct packet_rts rts;
    struct packet_rtr rtr;
    char buffer[0];
  };
};

struct __attribute__((packed)) lc_packet {
  struct packet_context context;
  struct packet_data data;
};

#endif
