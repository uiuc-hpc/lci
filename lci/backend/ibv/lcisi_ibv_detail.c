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

bool select_best_device_port(struct ibv_device** dev_list, int num_devices,
                             struct ibv_device** device_o, uint8_t* port_o)
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
      LCI_Log(LCI_LOG_INFO, "ibv", "Couldn't get context for %s.\n",
              ibv_get_device_name(device));
      continue;
    }

    // query device attribute
    struct ibv_device_attr dev_attr;
    int ret = ibv_query_device(dev_ctx, &dev_attr);
    if (ret != 0) {
      LCI_Log(LCI_LOG_INFO, "ibv", "Unable to query device %s.\n",
              ibv_get_device_name(device));
      goto close_device;
    }

    // query port attribute
    // port number starts from 1
    struct ibv_port_attr port_attr;
    for (uint8_t port_num = 1; port_num <= dev_attr.phys_port_cnt; port_num++) {
      ret = ibv_query_port(dev_ctx, port_num, &port_attr);
      if (ret != 0) {
        LCI_Log(LCI_LOG_INFO, "ibv", "Unable to query port (%s:%d).\n",
                ibv_get_device_name(device), port_num);
        continue;
      }
      // Check whether the port is active
      if (port_attr.state != IBV_PORT_ACTIVE) {
        LCI_Log(LCI_LOG_INFO, "ibv", "%s:%d is not active (state: %d).\n",
                ibv_get_device_name(device), port_num, port_attr.state);
        continue;
      }
      // Check whether we can get its lid
      if (port_attr.link_layer == IBV_LINK_LAYER_INFINIBAND && !port_attr.lid) {
        fprintf(stderr, "Couldn't get local LID\n");
        continue;
      }
      // Calculate its speed
      int width = translate_width(port_attr.active_width);
      if (width <= 0) {
        LCI_Log(LCI_LOG_INFO, "ibv", "%s:%d invalid width %d (%d).\n",
                ibv_get_device_name(device), port_num, width,
                port_attr.active_width);
        continue;
      }
      double speed = translate_speed(port_attr.active_speed);
      if (speed <= 0) {
        LCI_Log(LCI_LOG_INFO, "ibv", "%s:%d invalid speed %f (%d).\n",
                ibv_get_device_name(device), port_num, speed,
                port_attr.active_width);
        continue;
      }
      double total_speed = speed * width;
      LCI_Log(LCI_LOG_INFO, "ibv", "%s:%d speed is %.f (%d x %f).\n",
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
    LCI_Log(LCI_LOG_INFO, "ibv", "Select the best device %s:%d.\n",
            ibv_get_device_name(best_device), best_port);
    return true;
  } else {
    LCI_Log(LCI_LOG_INFO, "ibv", "No device is available!\n");
    return false;
  }
}

typedef enum roce_version_t {
  ROCE_V1,
  ROCE_V2,
  ROCE_VER_UNKNOWN
} roce_version_t;

roce_version_t query_gid_roce_version(LCISI_server_t* server,
                                      unsigned gid_index)
{
  char buf[16];
  int ret;
  char* dev_name = ibv_get_device_name(server->ib_dev);

  union ibv_gid gid;
  ret = ibv_query_gid(server->dev_ctx, server->dev_port, gid_index, &gid);
  if (ret == 0) {
    ret = LCT_read_file(buf, sizeof(buf),
                        "/sys/class/infiniband/%s/ports/%d/gid_attrs/types/%d",
                        dev_name, server->dev_port, gid_index);
    if (ret > 0) {
      if (!strncmp(buf, "IB/RoCE v1", 10)) {
        return ROCE_V1;
      } else if (!strncmp(buf, "RoCE v2", 7)) {
        return ROCE_V2;
      }
    }
  }
  LCI_Log(LCI_LOG_DEBUG, "ibv",
          "failed to parse gid type '%s' (dev=%s port=%d index=%d)\n", buf,
          dev_name, server->dev_port, gid_index);
  return ROCE_VER_UNKNOWN;
}

bool test_roce_gid_index(LCISI_server_t* server, uint8_t gid_index)
{
  struct ibv_ah_attr ah_attr;
  struct ibv_ah* ah;
  union ibv_gid gid;

  IBV_SAFECALL(
      ibv_query_gid(server->dev_ctx, server->dev_port, gid_index, &gid));

  memset(&ah_attr, 0, sizeof(ah_attr));
  ah_attr.port_num = server->dev_port;
  ah_attr.is_global = 1;
  ah_attr.grh.dgid = gid;
  ah_attr.grh.sgid_index = gid_index;
  ah_attr.grh.hop_limit = 255;
  ah_attr.grh.flow_label = 1;
  ah_attr.dlid = 0xC000;

  ah = ibv_create_ah(server->dev_pd, &ah_attr);
  if (ah == NULL) {
    LCI_Log(LCI_LOG_DEBUG, "ibv", "gid entry %d is not operational\n",
            gid_index);
    return false;
  }

  ibv_destroy_ah(ah);
  return true;
}

int select_best_gid_for_roce(LCISI_server_t* server)
{
  static const roce_version_t roce_prio[] = {
      ROCE_V2,
      ROCE_V1,
      ROCE_VER_UNKNOWN,
  };
  int gid_tbl_len = server->port_attr.gid_tbl_len;

  LCI_Log(LCI_LOG_DEBUG, "ibv", "RoCE gid auto selection among %d gids\n",
          gid_tbl_len);
  for (int prio_idx = 0; prio_idx < sizeof(roce_prio); prio_idx++) {
    for (int i = 0; i < gid_tbl_len; i++) {
      roce_version_t version = query_gid_roce_version(server, i);

      if ((roce_prio[prio_idx] == version) && test_roce_gid_index(server, i)) {
        LCI_Log(LCI_LOG_INFO, "ibv", "RoCE gid auto selection: use %d %d\n", i,
                version);
        return i;
      }
    }
  }

  const int default_gid = 0;
  LCI_Log(LCI_LOG_INFO, "ibv",
          "RoCE gid auto selection: fall back to the default gid %d\n",
          default_gid);
  return default_gid;  // default gid for roce
}

void gid_to_wire_gid(const union ibv_gid* gid, char wgid[])
{
  LCI_Assert(sizeof(union ibv_gid) == 16, "Unexpected ibv_gid size %d\n",
             sizeof(union ibv_gid));
  uint32_t tmp_gid[4];
  int i;

  memcpy(tmp_gid, gid, sizeof(tmp_gid));
  for (i = 0; i < 4; ++i) sprintf(&wgid[i * 8], "%08x", htobe32(tmp_gid[i]));
}

void wire_gid_to_gid(const char* wgid, union ibv_gid* gid)
{
  LCI_Assert(sizeof(union ibv_gid) == 16, "Unexpected ibv_gid size %d\n",
             sizeof(union ibv_gid));
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