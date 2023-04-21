#include "runtime/lcii.h"

// Lane width
static int translate_width(uint8_t width)
{
  switch (width) {
    case 1:
      return 1;
    case 2:
      return 4;
    case 4:
      return 8;
    case 8:
      return 12;
    case 16:
      return 2;
    default:
      return 0;
  }
}

// Per-lane speed (unit: Gbps)
static double translate_speed(uint8_t speed)
{
  switch (speed) {
    case 1:
      return 2.5;
    case 2:
      return 5;

    case 4: /* fall through */
    case 8:
      return 10;

    case 16:
      return 14;
    case 32:
      return 25;
    case 64:
      return 50;
    case 128:
      return 100;
    default:
      return 0;
  }
}

bool LCISI_ibv_select_best_device_port(struct ibv_device** dev_list,
                                       int num_devices,
                                       struct ibv_device** device_o,
                                       uint8_t* port_o)
{
  struct ibv_device* best_device;
  uint8_t best_port;
  double best_speed = 0;

  for (int i = 0; i < num_devices; ++i) {
    struct ibv_device* device = dev_list[i];

    // open the device
    struct ibv_context* dev_ctx;
    dev_ctx = ibv_open_device(device);
    if (!dev_ctx) {
      LCM_Log(LCM_LOG_INFO, "ibv", "Couldn't get context for %s.\n",
              ibv_get_device_name(device));
      continue;
    }

    // query device attribute
    struct ibv_device_attr dev_attr;
    int ret = ibv_query_device(dev_ctx, &dev_attr);
    if (ret != 0) {
      LCM_Log(LCM_LOG_INFO, "ibv", "Unable to query device %s.\n",
              ibv_get_device_name(device));
      goto close_device;
    }

    // query port attribute
    // port number starts from 1
    struct ibv_port_attr port_attr;
    for (uint8_t port_num = 1; port_num <= dev_attr.phys_port_cnt; port_num++) {
      ret = ibv_query_port(dev_ctx, port_num, &port_attr);
      if (ret != 0) {
        LCM_Log(LCM_LOG_INFO, "ibv", "Unable to query port (%s:%d).\n",
                ibv_get_device_name(device), port_num);
        continue;
      }
      // Check whether the port is active
      if (port_attr.state != IBV_PORT_ACTIVE) {
        LCM_Log(LCM_LOG_INFO, "ibv", "%s:%d is not active (state: %d).\n",
                ibv_get_device_name(device), port_num, port_attr.state);
        continue;
      }
      // Check whether we can get its lid
      if (port_attr.link_layer != IBV_LINK_LAYER_ETHERNET && !port_attr.lid) {
        fprintf(stderr, "Couldn't get local LID\n");
        continue;
      }
      // Calculate its speed
      int width = translate_width(port_attr.active_width);
      if (width <= 0) {
        LCM_Log(LCM_LOG_INFO, "ibv", "%s:%d invalid width %d (%d).\n",
                ibv_get_device_name(device), port_num, width,
                port_attr.active_width);
        continue;
      }
      double speed = translate_speed(port_attr.active_speed);
      if (speed <= 0) {
        LCM_Log(LCM_LOG_INFO, "ibv", "%s:%d invalid speed %f (%d).\n",
                ibv_get_device_name(device), port_num, speed,
                port_attr.active_width);
        continue;
      }
      double total_speed = speed * width;
      LCM_Log(LCM_LOG_INFO, "ibv", "%s:%d speed is %.f (%d x %f).\n",
              ibv_get_device_name(device), port_num, total_speed, width, speed);
      // Update the record if it is better.
      if (total_speed > best_speed) {
        best_speed = total_speed;
        best_device = device;
        best_port = port_num;
      }
    }

    // close the device
  close_device:
    ibv_close_device(dev_ctx);
  }
  if (best_speed > 0) {
    *device_o = best_device;
    *port_o = best_port;
    LCM_Log(LCM_LOG_INFO, "ibv", "Select the best device %s:%d.\n",
            ibv_get_device_name(best_device), best_port);
    return true;
  } else {
    LCM_Log(LCM_LOG_INFO, "ibv", "No device is available!\n");
    return false;
  }
}