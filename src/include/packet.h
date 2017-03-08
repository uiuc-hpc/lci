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

struct __attribute__((__packed__)) packet_header {
  int from;
  int tag;
  int size;
};

struct __attribute__((__packed__)) mv_rdz {
  uintptr_t sreq;
  uintptr_t tgt_addr;
  uint32_t rkey;
  uint32_t comm_id;
};

struct __attribute__((__packed__)) packet_data {
  struct packet_header header;
  union {
    struct mv_rdz rdz;
    char buffer[0];
  };
};

struct __attribute__((packed)) mv_packet {
  struct packet_context context;
  struct packet_data data;
};

#endif
