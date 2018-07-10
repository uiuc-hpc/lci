#ifndef PACKET_H_
#define PACKET_H_

#include "config.h"
#include <stdint.h>
#include "lc.h"

struct __attribute__((packed)) packet_context {
  // Most of the current ctx requires 128-bits (FIXME)
  uint64_t ctx1;
  uint64_t ctx2;
  // Here is LLCI context.
  struct lc_req req_s;
  struct lc_req* req;
  uintptr_t rma_mem;
  uint32_t proto;
  uint8_t runtime;
  uint16_t poolid;
};

struct __attribute__((__packed__)) packet_rts {
  uintptr_t req;
  uintptr_t src_addr;
  size_t size;
};

struct __attribute__((__packed__)) packet_rtr {
  uintptr_t req;
  uintptr_t src_addr;
  size_t size;
  uintptr_t tgt_addr;
  uint32_t rkey;
  uint32_t comm_id;
};

struct __attribute__((__packed__)) packet_data {
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
