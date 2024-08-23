#ifndef LCISI_IBV_DETAIL_H_
#define LCISI_IBV_DETAIL_H_
#include "infiniband/verbs.h"
#include <stdbool.h>

bool select_best_device_port(struct ibv_device** dev_list, int num_devices,
                             struct ibv_device** device_o, uint8_t* port_o);

void gid_to_wire_gid(const union ibv_gid* gid, char wgid[])
{
  uint32_t tmp_gid[4];
  int i;

  memcpy(tmp_gid, gid, sizeof(tmp_gid));
  for (i = 0; i < 4; ++i) sprintf(&wgid[i * 8], "%08x", htobe32(tmp_gid[i]));
}

void wire_gid_to_gid(const char* wgid, union ibv_gid* gid)
{
  char tmp[9];
  __be32 v32;
  int i;
  uint32_t tmp_gid[4];

  for (tmp[8] = 0, i = 0; i < 4; ++i) {
    memcpy(tmp, wgid + i * 8, 8);
    sscanf(tmp, "%x", &v32);
    tmp_gid[i] = be32toh(v32);
  }
  memcpy(gid, tmp_gid, sizeof(*gid));
}

#endif