#ifndef PACKET_H_
#define PACKET_H_

#include "config.h"
#include <stdint.h>

enum packetType {
  SEND_SHORT,
  SEND_READY,
  RECV_READY,
  SEND_WRITE_FIN,
  SEND_READY_FIN,
  SEND_AM
};

struct packet_header {
  enum packetType type;
  int8_t poolid;
  int from;
  int tag;
} __attribute__((aligned(8)));

struct mv_rdz {
  uintptr_t sreq;
  uintptr_t rreq;
  uintptr_t tgt_addr;
  uint32_t rkey;
};

union packet_content {
  char buffer[SHORT_MSG_SIZE];
  struct mv_rdz rdz;
};

struct packet {
  packet_header header;
  packet_content content;
} __attribute__((aligned(64)));

#endif
