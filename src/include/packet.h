#ifndef PACKET_H_
#define PACKET_H_

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

typedef struct {
    packet_header header;
    packet_content content;
} mv_packet_data;

struct mv_packet {
  struct {
    SERVER_CONTEXT;
  } context;
  mv_packet_data data;
} __attribute__((aligned(64)));

#endif
