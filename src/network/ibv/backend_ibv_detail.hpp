// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#ifndef LCISI_IBV_DETAIL_H_
#define LCISI_IBV_DETAIL_H_
#include "infiniband/verbs.h"
#include <stdbool.h>

namespace lci
{
namespace ibv_detail
{
bool select_best_device_port(struct ibv_device** dev_list, int num_devices,
                             struct ibv_device** device_o, uint8_t* port_o);

int select_best_gid_for_roce(struct ibv_device* ib_dev,
                             struct ibv_context* dev_ctx, struct ibv_pd* dev_pd,
                             struct ibv_port_attr port_attr, uint8_t dev_port);

const int WIRE_GID_NBYTES = 32;

void gid_to_wire_gid(const union ibv_gid* gid, char wgid[]);

void wire_gid_to_gid(const char* wgid, union ibv_gid* gid);
}  // namespace ibv_detail
}  // namespace lci

#endif