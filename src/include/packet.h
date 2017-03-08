#ifndef PACKET_H_
#define PACKET_H_

#include "config.h"
#include <stdint.h>

struct __attribute__((packed)) packet_context {
  SERVER_CONTEXT;
  uintptr_t req;
  uintptr_t rma_mem;
  uint64_t poolid;
  uint64_t proto;
};

struct __attribute__((packed)) mv_packet {
  struct packet_context context;
  struct packet_data data;
};

#endif
