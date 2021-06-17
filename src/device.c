#include "lcii.h"

void lc_dev_init(LCI_device_t *device_ptr)
{
  LCI_device_t device = LCIU_malloc(sizeof(struct LCI_device_s));
  *device_ptr = device;
  lc_server_init(device, &device->server);

  LCII_mt_init(&device->mt, 0);

  const size_t heap_size = LC_CACHE_LINE + LC_SERVER_NUM_PKTS * LC_PACKET_SIZE + LCI_REGISTERED_SEGMENT_SIZE;
  LCI_error_t ret = LCI_lbuffer_memalign(device, heap_size, 4096, &device->heap);
  LCM_Assert(ret == LCI_OK, "Device heap memory allocation failed\n");
  uintptr_t base_addr = (uintptr_t) device->heap.address;

  uintptr_t base_packet;
  LCM_Assert(sizeof(struct packet_context) <= LC_CACHE_LINE, "Unexpected packet_context size\n");
  base_packet = base_addr + LC_CACHE_LINE - sizeof(struct packet_context);

  lc_pool_create(&device->pkpool);
  for (int i = 0; i < LC_SERVER_NUM_PKTS; i++) {
    lc_packet* p = (lc_packet*)(base_packet + i * LC_PACKET_SIZE);
    p->context.pkpool = device->pkpool;
    p->context.poolid = 0;
    lc_pool_put(device->pkpool, p);
  }
}

void lc_dev_finalize(LCI_device_t device)
{
  int total_num = lc_pool_count(device->pkpool) +
                  lc_server_recv_posted_num(device->server);
  if (total_num != LC_SERVER_NUM_PKTS)
    LCM_DBG_Log(LCM_LOG_WARN, "Potentially losing packets %d != %d\n", total_num, LC_SERVER_NUM_PKTS);
  LCII_mt_free(&device->mt);
  LCI_lbuffer_free(device->heap);
  lc_pool_destroy(device->pkpool);
  lc_server_finalize(device->server);
  LCIU_free(device);
}