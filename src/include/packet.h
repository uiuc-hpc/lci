#ifndef PACKET_H_
#define PACKET_H_

#include "config.h"
#include <stdint.h>

struct __attribute__((packed)) packet_context {
  SERVER_CONTEXT;
  uint32_t pid; // keep this 32-bit.
  uint32_t poolid;
};

struct __attribute__((packed)) mv_packet {
  struct packet_context context;
  mv_packet_data_t data;
} __attribute__((aligned(64)));

#endif
