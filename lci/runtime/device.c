#include "runtime/lcii.h"

LCI_error_t LCII_fill_rq(LCII_endpoint_t* endpoint, bool block);

void LCII_endpoint_init(LCI_device_t device, bool single_threaded,
                        LCII_endpoint_t** endpoint_pp)
{
  // This is not LCI_endpoint_t which is just a wrapper of parameters,
  // but LCII_endpoint_t which maps to an underlying network context.
  LCII_endpoint_t* endpoint_p = LCIU_malloc(sizeof(LCII_endpoint_t));
  *endpoint_pp = endpoint_p;
#ifdef LCI_ENABLE_MULTITHREAD_PROGRESS
  atomic_init(&endpoint_p->recv_posted, 0);
#else
  endpoint_p->recv_posted = 0;
#endif
  endpoint_p->device = device;
  LCIS_endpoint_init(device->server, &endpoint_p->endpoint, single_threaded);
}

void LCII_endpoint_fina(LCII_endpoint_t** endpoint_pp)
{
  LCII_endpoint_t* endpoint_p = *endpoint_pp;
  LCIS_endpoint_fina(endpoint_p->endpoint);
  LCIU_free(endpoint_p);
  *endpoint_pp = NULL;
}

int LCII_endpoint_get_recv_posted(LCII_endpoint_t* endpoint_p)
{
#ifdef LCI_ENABLE_MULTITHREAD_PROGRESS
  return atomic_load(&endpoint_p->recv_posted);
#else
  return endpoint_p->recv_posted;
#endif
}

LCI_error_t LCI_device_init(LCI_device_t* device_ptr)
{
  LCI_device_t device = LCIU_malloc(sizeof(struct LCI_device_s));
  *device_ptr = device;

  bool single_threaded_prg = true;
#ifdef LCI_ENABLE_MULTITHREAD_PROGRESS
  single_threaded_prg = false;
#endif
  LCIS_server_init(device, &device->server);
  LCII_endpoint_init(device, false, &device->endpoint_worker);
  if (LCI_ENABLE_PRG_NET_ENDPOINT) {
    LCII_endpoint_init(device, single_threaded_prg, &device->endpoint_progress);
  } else {
    device->endpoint_progress = device->endpoint_worker;
  }
  if (LCI_USE_DREG) {
    LCII_rcache_init(device);
  }

  LCII_matchtable_create(&device->mt);
  LCM_archive_init(&(device->ctx_archive), 16);
  LCII_bq_init(&device->bq);
  LCIU_spinlock_init(&device->bq_spinlock);

  const size_t heap_size =
      LCI_CACHE_LINE + (size_t)LCI_SERVER_NUM_PKTS * LCI_PACKET_SIZE;
  LCI_error_t ret =
      LCI_lbuffer_memalign(device, heap_size, LCI_PAGESIZE, &device->heap);
  LCI_Assert(ret == LCI_OK, "Device heap memory allocation failed\n");
  uintptr_t base_addr = (uintptr_t)device->heap.address;

  uintptr_t base_packet;
  LCI_Assert(sizeof(struct LCII_packet_context) <= LCI_CACHE_LINE,
             "Unexpected packet_context size\n");
  base_packet = base_addr + LCI_CACHE_LINE - sizeof(struct LCII_packet_context);
  LCI_Assert(LCI_PACKET_SIZE % LCI_CACHE_LINE == 0,
             "The size of packets should be a multiple of cache line size\n");

  LCII_pool_create(&device->pkpool);
  for (size_t i = 0; i < LCI_SERVER_NUM_PKTS; i++) {
    LCII_packet_t* packet = (LCII_packet_t*)(base_packet + i * LCI_PACKET_SIZE);
    LCI_Assert(((uint64_t) & (packet->data)) % LCI_CACHE_LINE == 0,
               "packet.data is not well-aligned\n");
    packet->context.pkpool = device->pkpool;
    packet->context.poolid = 0;
#ifdef LCI_DEBUG
    packet->context.isInPool = true;
#endif
    LCII_pool_put(device->pkpool, packet);
  }
  LCI_Assert(LCI_SERVER_NUM_PKTS > 2 * LCI_SERVER_MAX_RECVS,
             "The packet number is too small!\n");
  if (LCI_ENABLE_PRG_NET_ENDPOINT)
    LCII_fill_rq(device->endpoint_progress, true);
  LCII_fill_rq(device->endpoint_worker, true);
  LCI_barrier();
  LCI_Log(LCI_LOG_INFO, "device", "device %p initialized\n", device);
  return LCI_OK;
}

LCI_error_t LCI_device_free(LCI_device_t* device_ptr)
{
  LCI_device_t device = *device_ptr;
  LCI_Log(LCI_LOG_INFO, "device", "free device %p\n", device);
  LCI_barrier();
  int total_recv_posted =
      LCII_endpoint_get_recv_posted(device->endpoint_worker);
  if (LCI_ENABLE_PRG_NET_ENDPOINT) {
    total_recv_posted +=
        LCII_endpoint_get_recv_posted(device->endpoint_progress);
  }
  int total_num = LCII_pool_count(device->pkpool) + total_recv_posted;
  if (total_num != LCI_SERVER_NUM_PKTS)
    LCI_Warn("Potentially losing packets %d != %d\n", total_num,
             LCI_SERVER_NUM_PKTS);
  LCII_matchtable_free(&device->mt);
  LCM_archive_fini(&(device->ctx_archive));
  LCII_bq_fini(&device->bq);
  LCIU_spinlock_fina(&device->bq_spinlock);
  LCI_lbuffer_free(device->heap);
  LCII_pool_destroy(device->pkpool);
  if (LCI_USE_DREG) {
    LCII_rcache_fina(device);
  }
  if (LCI_ENABLE_PRG_NET_ENDPOINT) {
    LCII_endpoint_fina(&device->endpoint_progress);
  }
  LCII_endpoint_fina(&device->endpoint_worker);
  LCIS_server_fina(device->server);
  LCIU_free(device);
  *device_ptr = NULL;
  return LCI_OK;
}