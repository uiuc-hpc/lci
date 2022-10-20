#include "lcii.h"

LCI_error_t LCI_device_init(LCI_device_t * device_ptr)
{
  LCI_device_t device = LCIU_malloc(sizeof(struct LCI_device_s));
  *device_ptr = device;
  device->recv_posted = 0;
  lc_server_init(device, &device->server);

  LCII_mt_init(&device->mt, 0);
  LCM_archive_init(&(device->ctx_archive), 16);
  LCII_bq_init(&device->bq);
  LCIU_spinlock_init(&device->bq_spinlock);

  const size_t heap_size = LCI_CACHE_LINE + (size_t) LCI_SERVER_MAX_PKTS * LCI_PACKET_SIZE + LCI_REGISTERED_SEGMENT_SIZE;
  LCI_error_t ret = LCI_lbuffer_memalign(device, heap_size, 4096, &device->heap);
  LCM_Assert(ret == LCI_OK, "Device heap memory allocation failed\n");
  uintptr_t base_addr = (uintptr_t) device->heap.address;

  uintptr_t base_packet;
  LCM_Assert(sizeof(struct packet_context) <= LCI_CACHE_LINE, "Unexpected packet_context size\n");
  base_packet = base_addr + LCI_CACHE_LINE - sizeof(struct packet_context);
  LCM_Assert(LCI_PACKET_SIZE % LCI_CACHE_LINE == 0, "The size of packets should be a multiple of cache line size\n");

  lc_pool_create(&device->pkpool);
  for (int i = 0; i < LCI_SERVER_MAX_PKTS; i++) {
    lc_packet* p = (lc_packet*)(base_packet + i * LCI_PACKET_SIZE );
    LCM_Assert(((uint64_t)&(p->data)) % LCI_CACHE_LINE == 0, "packet.data is not well-aligned\n");
    p->context.pkpool = device->pkpool;
    p->context.poolid = 0;
    lc_pool_put(device->pkpool, p);
  }
  LCM_DBG_Log_default(LCM_LOG_DEBUG, "device %p initialized\n", device);
  return LCI_OK;
}

LCI_error_t LCI_device_free(LCI_device_t *device_ptr)
{
  LCI_device_t device = *device_ptr;
  LCM_DBG_Log_default(LCM_LOG_DEBUG, "free device %p\n", device);
  int total_num = lc_pool_count(device->pkpool) + device->recv_posted;
  if (total_num != LCI_SERVER_MAX_PKTS)
    LCM_DBG_Log_default(LCM_LOG_WARN, "Potentially losing packets %d != %d\n", total_num, LCI_SERVER_MAX_PKTS);
  LCII_mt_free(&device->mt);
  LCM_archive_fini(&(device->ctx_archive));
  LCII_bq_fini(&device->bq);
  LCIU_spinlock_fina(&device->bq_spinlock);
  LCI_lbuffer_free(device->heap);
  lc_pool_destroy(device->pkpool);
  lc_server_finalize(device->server);
  LCIU_free(device);
  *device_ptr = NULL;
  return LCI_OK;
}

static inline LCI_error_t LCII_progress_bq(LCI_device_t device) {
  if (LCII_bq_is_empty(&device->bq))
    return LCI_ERR_RETRY;
  if (!LCIU_try_acquire_spinlock(&device->bq_spinlock))
    return LCI_ERR_RETRY;
  LCI_error_t ret = LCI_ERR_RETRY;
  LCII_bq_entry_t *entry = LCII_bq_top(&device->bq);
  if (entry != NULL) {
    if (entry->bqe_type == LCII_BQ_SENDS) {
      ret = lc_server_sends(entry->s, entry->rank, entry->buf, entry->size,
                           entry->meta);
    } else if (entry->bqe_type == LCII_BQ_SEND) {
      ret = lc_server_send(entry->s, entry->rank, entry->buf, entry->size,
                           entry->mr, entry->meta, entry->ctx);
    } else if (entry->bqe_type == LCII_BQ_PUT) {
      ret = lc_server_put(entry->s, entry->rank, entry->buf, entry->size,
                          entry->mr, entry->base, entry->offset, entry->rkey,
                          entry->ctx);
    } else if (entry->bqe_type == LCII_BQ_PUTIMM) {
      ret = lc_server_putImm(entry->s, entry->rank, entry->buf, entry->size,
                          entry->mr, entry->base, entry->offset, entry->rkey,
                          entry->meta, entry->ctx);
    } else {
      LCM_DBG_Assert(false, "Unknown bqe_type (%d)!\n", entry->bqe_type);
    }
    if (ret == LCI_OK) {
      LCII_bq_pop(&device->bq);
      if (entry->bqe_type == LCII_BQ_SENDS)
        LCIU_free(entry->buf);
    }
  }
  LCIU_release_spinlock(&device->bq_spinlock);
  return ret;
}

LCI_error_t LCI_progress(LCI_device_t device)
{
  int ret = LCI_ERR_RETRY;
  LCIS_cq_entry_t entry[LCI_CQ_MAX_POLL];
  int count = LCID_poll_cq(device->server, entry);
  LCM_DBG_Assert(count >= 0, "ibv_poll_cq returns error %d\n", count);
  if (count > 0) {
    ret = LCI_OK;
  }
  for (int i = 0; i < count; i++) {
    if (entry[i].opcode == LCII_OP_RECV) {
      // two-sided recv.
      LCM_DBG_Log_default(LCM_LOG_DEBUG, "complete recv: packet %p rank %d length %lu imm_data %u\n",
                  entry[i].ctx, entry[i].rank, entry[i].length, entry[i].imm_data);
      lc_serve_recv((lc_packet*)entry[i].ctx, entry[i].rank, entry[i].length, entry[i].imm_data);
      --device->recv_posted;
    } else if (entry[i].opcode == LCII_OP_RDMA_WRITE) {
      LCM_DBG_Log_default(LCM_LOG_DEBUG, "complete write: imm_data %u\n", entry[i].imm_data);
      if (entry[i].ctx != NULL) {
        LCII_free_packet((lc_packet*)entry[i].ctx);
        --device->recv_posted;
      }
      lc_serve_rdma(entry[i].imm_data);
    } else {
      // entry[i].opcode == LCII_OP_SEND
      LCM_DBG_Log_default(LCM_LOG_DEBUG, "complete send: address %p\n", (void*) entry[i].ctx);
      if (entry[i].ctx == NULL) continue;
      lc_serve_send((void*)entry[i].ctx);
    }
  }
  // make progress on backlog queue
  while (LCII_progress_bq(device) == LCI_OK) {
    ret = LCI_OK;
  }
  // Make sure we always have enough packet, but do not block.
  while (device->recv_posted < LCI_SERVER_MAX_RCVS) {
    lc_packet *packet = lc_pool_get_nb(device->pkpool);
    if (packet == NULL) {
      if (device->recv_posted < LCI_SERVER_MAX_RCVS / 2 && !g_server_no_recv_packets) {
        g_server_no_recv_packets = 1;
        LCM_DBG_Log_default(LCM_LOG_DEBUG, "WARNING-LC: deadlock alert. There is only "
                                   "%d packets left for post_recv\n",
                    device->recv_posted);
      }
      break;
    }
    packet->context.poolid = lc_pool_get_local(device->pkpool);
    lc_server_post_recv(device->server, packet->data.address, LCI_MEDIUM_SIZE,
                        *(device->heap.segment), packet);
    ++device->recv_posted;
  }
  if (device->recv_posted == LCI_SERVER_MAX_RCVS && g_server_no_recv_packets) {
    g_server_no_recv_packets = 0;
    LCM_DBG_Log_default(LCM_LOG_DEBUG, "WARNING-LC: recovered from deadlock alert.\n");
  }

  return ret;
}