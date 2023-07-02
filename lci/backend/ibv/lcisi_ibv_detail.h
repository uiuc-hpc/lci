#ifndef LCISI_IBV_DETAIL_H_
#define LCISI_IBV_DETAIL_H_
#include "infiniband/verbs.h"
#include <stdbool.h>

bool LCISI_ibv_select_best_device_port(struct ibv_device** dev_list,
                                       int num_devices,
                                       struct ibv_device** device_o,
                                       uint8_t* port_o);

#endif