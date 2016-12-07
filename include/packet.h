#ifndef PACKET_H_
#define PACKET_H_

#include "config.h"
#include <stdint.h>

typedef struct packet_header {
  int32_t fid;
  int8_t poolid;
  int from;
  int tag;
} packet_header __attribute__((aligned(8)));

struct mv_rdz {
  uintptr_t sreq;
  uintptr_t rreq;
  uintptr_t tgt_addr;
  uint32_t rkey;
};

typedef union packet_content {
  char buffer[SHORT_MSG_SIZE];
  struct mv_rdz rdz;
} packet_content;

struct mv_packet {
  packet_header header;
  packet_content content;
} __attribute__((aligned(64)));

#endif
