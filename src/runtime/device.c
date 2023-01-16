#include "runtime/lcii.h"

LCI_error_t LCI_device_init(LCI_device_t* device_ptr)
{
  LCI_device_t device = LCIU_malloc(sizeof(struct LCI_device_s));
  *device_ptr = device;
  LCIS_server_init(device, &device->server);
  device->endpoint_progress.recv_posted = 0;
  device->endpoint_progress.device = device;
  LCIS_endpoint_init(device->server, &device->endpoint_progress.endpoint,
                     LCI_SINGLE_THREAD_PROGRESS);
  device->endpoint_worker.recv_posted = 0;
  device->endpoint_worker.device = device;
  LCIS_endpoint_init(device->server, &device->endpoint_worker.endpoint, false);
  if (LCI_USE_DREG) {
    LCII_rcache_init(device);
  }

  LCII_mt_init(&device->mt, 0);
  LCM_archive_init(&(device->ctx_archive), 16);
  LCII_bq_init(&device->bq);
  LCIU_spinlock_init(&device->bq_spinlock);

  const size_t heap_size = LCI_CACHE_LINE +
                           (size_t)LCI_SERVER_NUM_PKTS * LCI_PACKET_SIZE +
                           LCI_REGISTERED_SEGMENT_SIZE;
  LCI_error_t ret =
      LCI_lbuffer_memalign(device, heap_size, LCI_PAGESIZE, &device->heap);
  LCM_Assert(ret == LCI_OK, "Device heap memory allocation failed\n");
  uintptr_t base_addr = (uintptr_t)device->heap.address;

  uintptr_t base_packet;
  LCM_Assert(sizeof(struct LCII_packet_context) <= LCI_CACHE_LINE,
             "Unexpected packet_context size\n");
  base_packet = base_addr + LCI_CACHE_LINE - sizeof(struct LCII_packet_context);
  LCM_Assert(LCI_PACKET_SIZE % LCI_CACHE_LINE == 0,
             "The size of packets should be a multiple of cache line size\n");

  LCII_pool_create(&device->pkpool);
  for (size_t i = 0; i < LCI_SERVER_NUM_PKTS; i++) {
    LCII_packet_t* p = (LCII_packet_t*)(base_packet + i * LCI_PACKET_SIZE);
    LCM_Assert(((uint64_t) & (p->data)) % LCI_CACHE_LINE == 0,
               "packet.data is not well-aligned\n");
    p->context.pkpool = device->pkpool;
    p->context.poolid = 0;
    LCII_pool_put(device->pkpool, p);
  }
  device->did_work_consecutive = 0;
  LCM_DBG_Log(LCM_LOG_DEBUG, "device", "device %p initialized\n", device);
  return LCI_OK;
}

LCI_error_t LCI_device_free(LCI_device_t* device_ptr)
{
  LCI_device_t device = *device_ptr;
  LCM_DBG_Log(LCM_LOG_DEBUG, "device", "free device %p\n", device);
  int total_num = LCII_pool_count(device->pkpool) +
                  device->endpoint_progress.recv_posted +
                  device->endpoint_worker.recv_posted;
  if (total_num != LCI_SERVER_NUM_PKTS)
    LCM_Warn("Potentially losing packets %d != %d\n", total_num,
             LCI_SERVER_NUM_PKTS);
  LCII_mt_free(&device->mt);
  LCM_archive_fini(&(device->ctx_archive));
  LCII_bq_fini(&device->bq);
  LCIU_spinlock_fina(&device->bq_spinlock);
  LCI_lbuffer_free(device->heap);
  LCII_pool_destroy(device->pkpool);
  if (LCI_USE_DREG) {
    LCII_rcache_fina(device);
  }
  LCIS_endpoint_fina(device->endpoint_worker.endpoint);
  LCIS_endpoint_fina(device->endpoint_progress.endpoint);
  LCIS_server_fina(device->server);
  LCIU_free(device);
  *device_ptr = NULL;
  return LCI_OK;
}