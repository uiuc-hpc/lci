#ifndef PACKET_H_
#define PACKET_H_

#include <stdint.h>
#include "config.h"

struct mv_packet {
  struct {
    SERVER_CONTEXT;
  } context;
  mv_packet_data_t data;
} __attribute__((aligned(64)));

#endif
