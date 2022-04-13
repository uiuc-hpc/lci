#ifndef LCI_LCII_RDV_H
#define LCI_LCII_RDV_H

// wrapper for send and put
static inline void lc_server_send_bq(LCII_backlog_queue_t *bq_p,
                                     LCIU_spinlock_t *bq_spinlock_p,
                                     LCIS_server_t s, int rank, void* buf,
                                     size_t size, LCIS_mr_t mr,
                                     LCIS_meta_t meta,
                                     void* ctx) {
  if (LCII_bq_is_empty(bq_p)) {
    LCI_error_t ret = lc_server_send(s, rank, buf, size, mr, meta, ctx);
    if (ret == LCI_OK) return;
    else {
      LCM_Assert(ret == LCI_ERR_RETRY, "fatal error!\n");
    }
  }
  // push to backlog queue
  LCII_bq_entry_t *entry = LCIU_malloc(sizeof(struct LCII_bq_entry_t));
  entry->bqe_type = LCII_BQ_SEND;
  entry->s = s;
  entry->rank = rank;
  entry->buf = buf;
  entry->size = size;
  entry->mr = mr;
  entry->meta = meta;
  entry->ctx = ctx;
  LCIU_acquire_spinlock(bq_spinlock_p);
  LCII_bq_push(bq_p, entry);
  LCIU_release_spinlock(bq_spinlock_p);
}

static inline void lc_server_put_bq(LCII_backlog_queue_t *bq_p,
                                    LCIU_spinlock_t *bq_spinlock_p,
                                    LCIS_server_t s, int rank, void* buf,
                                    size_t size, LCIS_mr_t mr, uintptr_t base,
                                    uint32_t offset,LCIS_rkey_t rkey,
                                    LCIS_meta_t meta, void* ctx) {
  if (LCII_bq_is_empty(bq_p)) {
    LCI_error_t ret = lc_server_put(s, rank, buf, size, mr, base, offset, rkey, meta, ctx);
    if (ret == LCI_OK) return;
    else {
      LCM_Assert(ret == LCI_ERR_RETRY, "fatal error!\n");
    }
  }
  // push to backlog queue
  LCII_bq_entry_t *entry = LCIU_malloc(sizeof(struct LCII_bq_entry_t));
  entry->bqe_type = LCII_BQ_PUT;
  entry->s = s;
  entry->rank = rank;
  entry->buf = buf;
  entry->size = size;
  entry->mr = mr;
  entry->meta = meta;
  entry->ctx = ctx;
  entry->base = base;
  entry->offset = offset;
  entry->rkey = rkey;
  LCIU_acquire_spinlock(bq_spinlock_p);
  LCII_bq_push(bq_p, entry);
  LCIU_release_spinlock(bq_spinlock_p);
}

static inline void LCII_handle_2sided_rts(LCI_endpoint_t ep, lc_packet* packet, LCII_context_t *rdv_ctx)
{

  LCM_DBG_Assert(rdv_ctx->data.lbuffer.address == NULL ||
                 rdv_ctx->data.lbuffer.length >= packet->data.rts.size,
                 "the message sent by sendl (%lu) is larger than the buffer posted by recvl (%lu)!\n",
                 packet->data.rts.size, rdv_ctx->data.lbuffer.length);
  rdv_ctx->rank = packet->context.src_rank;
  rdv_ctx->data.lbuffer.length = packet->data.rts.size;

  LCII_context_t *rtr_ctx = LCIU_malloc(sizeof(LCII_context_t));
  rtr_ctx->data.mbuffer.address = &(packet->data);
  LCII_initilize_comp_attr(rtr_ctx->comp_attr);
  LCII_comp_attr_set_free_packet(rtr_ctx->comp_attr, 1);

  packet->context.poolid = -1;
  uint64_t ctx_key;
  int result = LCM_archive_put(ep->ctx_archive_p, (uintptr_t)rdv_ctx, &ctx_key);
  // TODO: be able to pass back pressure to user
  LCM_Assert(result == LCM_SUCCESS, "Archive is full!\n");
  packet->data.rtr.recv_ctx_key = ctx_key;
  if (rdv_ctx->data.lbuffer.address == NULL) {
    LCI_lbuffer_alloc(ep->device, packet->data.rts.size, &rdv_ctx->data.lbuffer);
  }
  if (rdv_ctx->data.lbuffer.segment == LCI_SEGMENT_ALL) {
    LCM_DBG_Assert(LCII_comp_attr_get_dereg(rdv_ctx->comp_attr) == 1, "\n");
    LCI_memory_register(ep->device, rdv_ctx->data.lbuffer.address,
                        rdv_ctx->data.lbuffer.length,
                        &rdv_ctx->data.lbuffer.segment);
  } else {
    LCM_DBG_Assert(LCII_comp_attr_get_dereg(rdv_ctx->comp_attr) == 0, "\n");
  }
  packet->data.rtr.remote_addr_base = (uintptr_t) rdv_ctx->data.lbuffer.segment->address;
  packet->data.rtr.remote_addr_offset =
      (uintptr_t) rdv_ctx->data.lbuffer.address - packet->data.rtr.remote_addr_base;
  packet->data.rtr.rkey = lc_server_rma_rkey(*(rdv_ctx->data.lbuffer.segment));

  lc_server_send_bq(ep->bq_p, ep->bq_spinlock_p,
                    ep->device->server, rdv_ctx->rank, packet->data.address,
                    sizeof(struct packet_rtr), *(ep->device->heap.segment),
                    LCII_MAKE_PROTO(ep->gid, LCI_MSG_RTR, 0), rtr_ctx);

}

static inline void LCII_handle_2sided_rtr(LCI_endpoint_t ep, lc_packet* packet)
{
  LCII_context_t *ctx = (LCII_context_t*) packet->data.rtr.send_ctx;
  if (ctx->data.lbuffer.segment == LCI_SEGMENT_ALL) {
    LCM_DBG_Assert(LCII_comp_attr_get_dereg(ctx->comp_attr) == 1, "\n");
    LCI_memory_register(ep->device, ctx->data.lbuffer.address,
                        ctx->data.lbuffer.length,
                        &ctx->data.lbuffer.segment);
  } else {
    LCM_DBG_Assert(LCII_comp_attr_get_dereg(ctx->comp_attr) == 0, "\n");
  }
  lc_server_put_bq(ep->bq_p, ep->bq_spinlock_p, ep->device->server, ctx->rank,
                   ctx->data.lbuffer.address, ctx->data.lbuffer.length,
                   *(ctx->data.lbuffer.segment),
                   packet->data.rtr.remote_addr_base, packet->data.rtr.remote_addr_offset,
                   packet->data.rtr.rkey,
                   LCII_MAKE_PROTO(ep->gid, LCI_MSG_LONG, packet->data.rtr.recv_ctx_key),
                   ctx);
  LCII_free_packet(packet);
}

static inline void LCII_handle_2sided_writeImm(LCI_endpoint_t ep, uint64_t ctx_key)
{
  LCII_context_t *ctx =
      (LCII_context_t*)LCM_archive_remove(ep->ctx_archive_p, ctx_key);
  LCM_DBG_Assert(ctx->data_type == LCI_LONG,
                 "Didn't get the right context! This might imply some bugs in the LCM_archive_t.\n");
  LCM_DBG_Log(LCM_LOG_DEBUG, "complete recvl: ctx %p rank %d buf %p size %lu "
                             "tag %d user_ctx %p completion attr %x completion %p\n",
              ctx, ctx->rank, ctx->data.lbuffer.address, ctx->data.lbuffer.length,
              ctx->tag, ctx->user_context, ctx->comp_attr, ctx->completion);
  lc_ce_dispatch(ctx);
}


static inline void LCII_handle_1sided_rts(LCI_endpoint_t ep, lc_packet* packet,
                                          uint32_t src_rank, uint16_t tag)
{
  LCII_context_t *rdv_ctx = LCIU_malloc(sizeof(LCII_context_t));
  LCI_lbuffer_alloc(ep->device, packet->data.rts.size, &rdv_ctx->data.lbuffer);
  rdv_ctx->data_type = LCI_LONG;
  rdv_ctx->rank = src_rank;
  rdv_ctx->tag = tag;
  rdv_ctx->user_context = NULL;
  LCII_initilize_comp_attr(rdv_ctx->comp_attr);
  LCII_comp_attr_set_comp_type(rdv_ctx->comp_attr, LCI_COMPLETION_QUEUE);
  rdv_ctx->completion = ep->default_comp;

  LCII_context_t *rtr_ctx = LCIU_malloc(sizeof(LCII_context_t));
  rtr_ctx->data.mbuffer.address = &(packet->data);
  LCII_initilize_comp_attr(rtr_ctx->comp_attr);
  LCII_comp_attr_set_free_packet(rtr_ctx->comp_attr, 1);

  // reuse the rts packet as rtr packet
  packet->context.poolid = -1;
  uint64_t ctx_key;
  int result = LCM_archive_put(ep->ctx_archive_p, (uintptr_t)rdv_ctx, &ctx_key);
  // TODO: be able to pass back pressure to user
  LCM_Assert(result == LCM_SUCCESS, "Archive is full!\n");
  packet->data.rtr.recv_ctx_key = ctx_key;
  packet->data.rtr.remote_addr_base = (uintptr_t) rdv_ctx->data.lbuffer.segment->address;
  packet->data.rtr.remote_addr_offset =
      (uintptr_t) rdv_ctx->data.lbuffer.address - packet->data.rtr.remote_addr_base;
  packet->data.rtr.rkey = lc_server_rma_rkey(*(rdv_ctx->data.lbuffer.segment));

  LCM_DBG_Log(LCM_LOG_DEBUG, "send rtr: type %d sctx %p base %p offset %d "
              "rkey %lu rctx_key %u\n", packet->data.rtr.msg_type,
              (void*) packet->data.rtr.send_ctx, (void*) packet->data.rtr.remote_addr_base,
              packet->data.rtr.remote_addr_offset, packet->data.rtr.rkey,
              packet->data.rtr.recv_ctx_key);
  lc_server_send_bq(ep->bq_p, ep->bq_spinlock_p,
                    ep->device->server, rdv_ctx->rank, packet->data.address,
                    sizeof(struct packet_rtr), *(ep->device->heap.segment),
                    LCII_MAKE_PROTO(ep->gid, LCI_MSG_RTR, 0), rtr_ctx);
}

static inline void LCII_handle_1sided_rtr(LCI_endpoint_t ep, lc_packet* packet)
{
  LCII_context_t *ctx = (LCII_context_t*) packet->data.rtr.send_ctx;
  if (ctx->data.lbuffer.segment == LCI_SEGMENT_ALL) {
    LCM_DBG_Assert(LCII_comp_attr_get_dereg(ctx->comp_attr) == 1, "\n");
    LCI_memory_register(ep->device, ctx->data.lbuffer.address,
                        ctx->data.lbuffer.length,
                        &ctx->data.lbuffer.segment);
  } else {
    LCM_DBG_Assert(LCII_comp_attr_get_dereg(ctx->comp_attr) == 0, "\n");
  }
  lc_server_put_bq(ep->bq_p, ep->bq_spinlock_p, ep->device->server, ctx->rank,
                   ctx->data.lbuffer.address, ctx->data.lbuffer.length,
                   *(ctx->data.lbuffer.segment),
                   packet->data.rtr.remote_addr_base, packet->data.rtr.remote_addr_offset,
                   packet->data.rtr.rkey,
                   LCII_MAKE_PROTO(ep->gid, LCI_MSG_RDMA_LONG, packet->data.rtr.recv_ctx_key),
                   ctx);
  LCII_free_packet(packet);
}

static inline void LCII_handle_1sided_writeImm(LCI_endpoint_t ep, uint64_t ctx_key)
{
  LCII_context_t *ctx =
      (LCII_context_t*)LCM_archive_remove(ep->ctx_archive_p, ctx_key);
  LCM_DBG_Assert(ctx->data_type == LCI_LONG,
                 "Didn't get the right context! This might imply some bugs in the LCM_archive_t.\n");
  // recvl has been completed locally. Need to process completion.
  lc_ce_dispatch(ctx);
}

#endif  // LCI_LCII_RDV_H
