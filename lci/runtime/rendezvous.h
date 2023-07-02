#ifndef LCI_RENDEZVOUS_H
#define LCI_RENDEZVOUS_H

// wrapper for send and put
static inline void LCIS_post_sends_bq(LCII_backlog_queue_t* bq_p,
                                      LCIU_spinlock_t* bq_spinlock_p,
                                      LCIS_endpoint_t endpoint, int rank,
                                      void* buf, size_t size, LCIS_meta_t meta)
{
  if (LCII_bq_is_empty(bq_p)) {
    LCI_error_t ret = LCIS_post_sends(endpoint, rank, buf, size, meta);
    if (ret == LCI_OK)
      return;
    else {
      LCI_Assert(ret == LCI_ERR_RETRY, "fatal error!\n");
    }
  }
  // push to backlog queue
  LCI_DBG_Log(LCI_LOG_TRACE, "bq",
              "Pushed to backlog queue (sends): "
              "post sends: rank %d buf %p size %lu meta %d\n",
              rank, buf, size, meta);
  LCII_bq_entry_t* entry = LCIU_malloc(sizeof(struct LCII_bq_entry_t));
  entry->bqe_type = LCII_BQ_SENDS;
  entry->rank = rank;
  entry->buf = LCIU_malloc(size);
  memcpy(entry->buf, buf, size);
  entry->size = size;
  entry->meta = meta;
  LCIU_acquire_spinlock(bq_spinlock_p);
  LCII_bq_push(bq_p, entry);
  LCIU_release_spinlock(bq_spinlock_p);
}

static inline void LCIS_post_send_bq(LCII_backlog_queue_t* bq_p,
                                     LCIU_spinlock_t* bq_spinlock_p,
                                     LCIS_endpoint_t endpoint, int rank,
                                     void* buf, size_t size, LCIS_mr_t mr,
                                     LCIS_meta_t meta, void* ctx)
{
  if (LCII_bq_is_empty(bq_p)) {
    LCI_error_t ret = LCIS_post_send(endpoint, rank, buf, size, mr, meta, ctx);
    if (ret == LCI_OK)
      return;
    else {
      LCI_Assert(ret == LCI_ERR_RETRY, "fatal error!\n");
    }
  }
  // push to backlog queue
  LCI_DBG_Log(LCI_LOG_TRACE, "bq",
              "Pushed to backlog queue (send): "
              "rank %d buf %p size %lu mr %p meta %d ctx %p\n",
              rank, buf, size, mr.mr_p, meta, ctx);
  LCII_bq_entry_t* entry = LCIU_malloc(sizeof(struct LCII_bq_entry_t));
  entry->bqe_type = LCII_BQ_SEND;
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

static inline void LCIS_post_put_bq(LCII_backlog_queue_t* bq_p,
                                    LCIU_spinlock_t* bq_spinlock_p,
                                    LCIS_endpoint_t endpoint, int rank,
                                    void* buf, size_t size, LCIS_mr_t mr,
                                    uintptr_t base, LCIS_offset_t offset,
                                    LCIS_rkey_t rkey, void* ctx)
{
  if (LCII_bq_is_empty(bq_p)) {
    LCI_error_t ret =
        LCIS_post_put(endpoint, rank, buf, size, mr, base, offset, rkey, ctx);
    if (ret == LCI_OK)
      return;
    else {
      LCI_Assert(ret == LCI_ERR_RETRY, "fatal error!\n");
    }
  }
  // push to backlog queue
  LCI_DBG_Log(LCI_LOG_TRACE, "bq",
              "Pushed to backlog queue (put): "
              "rank %d buf %p size %lu mr %p base %p "
              "offset %lu rkey %lu ctx %p\n",
              rank, buf, size, mr.mr_p, (void*)base, offset, rkey, ctx);
  LCII_bq_entry_t* entry = LCIU_malloc(sizeof(struct LCII_bq_entry_t));
  entry->bqe_type = LCII_BQ_PUT;
  entry->rank = rank;
  entry->buf = buf;
  entry->size = size;
  entry->mr = mr;
  entry->ctx = ctx;
  entry->base = base;
  entry->offset = offset;
  entry->rkey = rkey;
  LCIU_acquire_spinlock(bq_spinlock_p);
  LCII_bq_push(bq_p, entry);
  LCIU_release_spinlock(bq_spinlock_p);
}

static inline void LCIS_post_putImm_bq(LCII_backlog_queue_t* bq_p,
                                       LCIU_spinlock_t* bq_spinlock_p,
                                       LCIS_endpoint_t endpoint, int rank,
                                       void* buf, size_t size, LCIS_mr_t mr,
                                       uintptr_t base, LCIS_offset_t offset,
                                       LCIS_rkey_t rkey, LCIS_meta_t meta,
                                       void* ctx)
{
  if (LCII_bq_is_empty(bq_p)) {
    LCI_error_t ret = LCIS_post_putImm(endpoint, rank, buf, size, mr, base,
                                       offset, rkey, meta, ctx);
    if (ret == LCI_OK)
      return;
    else {
      LCI_Assert(ret == LCI_ERR_RETRY, "fatal error!\n");
    }
  }
  // push to backlog queue
  LCI_DBG_Log(LCI_LOG_TRACE, "bq",
              "Pushed to backlog queue (putImm): "
              "rank %d buf %p size %lu mr %p base %p "
              "offset %lu rkey %lu meta %u ctx %p\n",
              rank, buf, size, mr.mr_p, (void*)base, offset, rkey, meta, ctx);
  LCII_bq_entry_t* entry = LCIU_malloc(sizeof(struct LCII_bq_entry_t));
  entry->bqe_type = LCII_BQ_PUTIMM;
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

static inline void LCII_handle_2sided_rts(LCI_endpoint_t ep,
                                          LCII_packet_t* packet,
                                          LCII_context_t* rdv_ctx,
                                          bool is_progress)
{
  LCI_DBG_Assert(rdv_ctx->data.lbuffer.address == NULL ||
                     rdv_ctx->data.lbuffer.length >= packet->data.rts.size,
                 "the message sent by sendl (%lu) is larger than the buffer "
                 "posted by recvl (%lu)!\n",
                 packet->data.rts.size, rdv_ctx->data.lbuffer.length);
  rdv_ctx->rank = packet->context.src_rank;
  rdv_ctx->data.lbuffer.length = packet->data.rts.size;

  LCII_context_t* rtr_ctx = LCIU_malloc(sizeof(LCII_context_t));
  rtr_ctx->data.mbuffer.address = &(packet->data);
  LCII_initilize_comp_attr(rtr_ctx->comp_attr);
  LCII_comp_attr_set_msg_type(rtr_ctx->comp_attr, LCI_MSG_RTR);
  LCII_comp_attr_set_free_packet(rtr_ctx->comp_attr, 1);

  packet->context.poolid = -1;
  uint64_t ctx_key;
  int result = LCM_archive_put(ep->ctx_archive_p, (uintptr_t)rdv_ctx, &ctx_key);
  // TODO: be able to pass back pressure to user
  LCI_Assert(result == LCM_SUCCESS, "Archive is full!\n");
  packet->data.rtr.recv_ctx_key = ctx_key;
  LCII_PCOUNTER_START(rts_mem_reg_timer);
  if (rdv_ctx->data.lbuffer.address == NULL) {
    LCI_lbuffer_alloc(ep->device, packet->data.rts.size,
                      &rdv_ctx->data.lbuffer);
  }
  if (rdv_ctx->data.lbuffer.segment == LCI_SEGMENT_ALL) {
    LCI_DBG_Assert(LCII_comp_attr_get_dereg(rdv_ctx->comp_attr) == 1, "\n");
    LCI_memory_register(ep->device, rdv_ctx->data.lbuffer.address,
                        rdv_ctx->data.lbuffer.length,
                        &rdv_ctx->data.lbuffer.segment);
  } else {
    LCI_DBG_Assert(LCII_comp_attr_get_dereg(rdv_ctx->comp_attr) == 0, "\n");
  }
  LCII_PCOUNTER_END(rts_mem_reg_timer);
  packet->data.rtr.remote_addr_base =
      (uintptr_t)rdv_ctx->data.lbuffer.segment->mr.address;
  packet->data.rtr.remote_addr_offset =
      (uintptr_t)rdv_ctx->data.lbuffer.address -
      packet->data.rtr.remote_addr_base;
  packet->data.rtr.rkey = LCIS_rma_rkey(rdv_ctx->data.lbuffer.segment->mr);

  LCIS_endpoint_t endpoint_to_use;
  if (is_progress) {
    endpoint_to_use = ep->device->endpoint_progress.endpoint;
  } else {
    endpoint_to_use = ep->device->endpoint_worker.endpoint;
  }
  LCII_PCOUNTER_START(rts_send_timer);
  LCIS_post_send_bq(ep->bq_p, ep->bq_spinlock_p, endpoint_to_use, rdv_ctx->rank,
                    packet->data.address, sizeof(struct LCII_packet_rtr_t),
                    ep->device->heap.segment->mr,
                    LCII_MAKE_PROTO(ep->gid, LCI_MSG_RTR, 0), rtr_ctx);
  LCII_PCOUNTER_END(rts_send_timer);
}

static inline void LCII_handle_2sided_rtr(LCI_endpoint_t ep,
                                          LCII_packet_t* packet)
{
  LCII_context_t* ctx = (LCII_context_t*)packet->data.rtr.send_ctx;
  LCII_PCOUNTER_START(rtr_mem_reg_timer);
  if (ctx->data.lbuffer.segment == LCI_SEGMENT_ALL) {
    LCI_DBG_Assert(LCII_comp_attr_get_dereg(ctx->comp_attr) == 1, "\n");
    LCI_memory_register(ep->device, ctx->data.lbuffer.address,
                        ctx->data.lbuffer.length, &ctx->data.lbuffer.segment);
  } else {
    LCI_DBG_Assert(LCII_comp_attr_get_dereg(ctx->comp_attr) == 0, "\n");
  }
  LCII_PCOUNTER_END(rtr_mem_reg_timer);
  LCII_PCOUNTER_START(rtr_putimm_timer);
  LCIS_post_putImm_bq(
      ep->bq_p, ep->bq_spinlock_p, ep->device->endpoint_progress.endpoint,
      ctx->rank, ctx->data.lbuffer.address, ctx->data.lbuffer.length,
      ctx->data.lbuffer.segment->mr, packet->data.rtr.remote_addr_base,
      packet->data.rtr.remote_addr_offset, packet->data.rtr.rkey,
      LCII_MAKE_PROTO(ep->gid, LCI_MSG_LONG, packet->data.rtr.recv_ctx_key),
      ctx);
  LCII_PCOUNTER_END(rtr_putimm_timer);
  LCII_free_packet(packet);
}

static inline void LCII_handle_2sided_writeImm(LCI_endpoint_t ep,
                                               uint64_t ctx_key)
{
  LCII_context_t* ctx =
      (LCII_context_t*)LCM_archive_remove(ep->ctx_archive_p, ctx_key);
  LCI_DBG_Assert(ctx->data_type == LCI_LONG,
                 "Didn't get the right context! This might imply some bugs in "
                 "the LCM_archive_t.\n");
  LCI_DBG_Log(LCI_LOG_TRACE, "rdv",
              "complete recvl: ctx %p rank %d buf %p size %lu "
              "tag %d user_ctx %p completion attr %x completion %p\n",
              ctx, ctx->rank, ctx->data.lbuffer.address,
              ctx->data.lbuffer.length, ctx->tag, ctx->user_context,
              ctx->comp_attr, ctx->completion);
  LCII_PCOUNTER_ADD(net_recv_comp, ctx->data.lbuffer.length);
  lc_ce_dispatch(ctx);
}

static inline void LCII_handle_1sided_rts(LCI_endpoint_t ep,
                                          LCII_packet_t* packet,
                                          uint32_t src_rank, uint16_t tag)
{
  LCII_context_t* rdv_ctx = LCIU_malloc(sizeof(LCII_context_t));
  LCI_lbuffer_alloc(ep->device, packet->data.rts.size, &rdv_ctx->data.lbuffer);
  rdv_ctx->data_type = LCI_LONG;
  rdv_ctx->rank = src_rank;
  rdv_ctx->tag = tag;
  rdv_ctx->user_context = NULL;
  LCII_initilize_comp_attr(rdv_ctx->comp_attr);
  LCII_comp_attr_set_msg_type(rdv_ctx->comp_attr, LCI_MSG_NONE);
  LCII_comp_attr_set_comp_type(rdv_ctx->comp_attr, ep->msg_comp_type);
  rdv_ctx->completion = ep->default_comp;

  LCII_context_t* rtr_ctx = LCIU_malloc(sizeof(LCII_context_t));
  rtr_ctx->data.mbuffer.address = &(packet->data);
  LCII_initilize_comp_attr(rtr_ctx->comp_attr);
  LCII_comp_attr_set_msg_type(rtr_ctx->comp_attr, LCI_MSG_RTR);
  LCII_comp_attr_set_free_packet(rtr_ctx->comp_attr, 1);

  // reuse the rts packet as rtr packet
  packet->context.poolid = -1;
  uint64_t ctx_key;
  int result = LCM_archive_put(ep->ctx_archive_p, (uintptr_t)rdv_ctx, &ctx_key);
  // TODO: be able to pass back pressure to user
  LCI_Assert(result == LCM_SUCCESS, "Archive is full!\n");
  packet->data.rtr.recv_ctx_key = ctx_key;
  packet->data.rtr.remote_addr_base =
      (uintptr_t)rdv_ctx->data.lbuffer.segment->mr.address;
  packet->data.rtr.remote_addr_offset =
      (uintptr_t)rdv_ctx->data.lbuffer.address -
      packet->data.rtr.remote_addr_base;
  packet->data.rtr.rkey = LCIS_rma_rkey(rdv_ctx->data.lbuffer.segment->mr);

  LCI_DBG_Log(LCI_LOG_TRACE, "rdv",
              "send rtr: type %d sctx %p base %p offset %lu "
              "rkey %lu rctx_key %u\n",
              packet->data.rtr.msg_type, (void*)packet->data.rtr.send_ctx,
              (void*)packet->data.rtr.remote_addr_base,
              packet->data.rtr.remote_addr_offset, packet->data.rtr.rkey,
              packet->data.rtr.recv_ctx_key);
  LCIS_post_send_bq(ep->bq_p, ep->bq_spinlock_p,
                    ep->device->endpoint_progress.endpoint, rdv_ctx->rank,
                    packet->data.address, sizeof(struct LCII_packet_rtr_t),
                    ep->device->heap.segment->mr,
                    LCII_MAKE_PROTO(ep->gid, LCI_MSG_RTR, 0), rtr_ctx);
}

static inline void LCII_handle_1sided_rtr(LCI_endpoint_t ep,
                                          LCII_packet_t* packet)
{
  LCII_context_t* ctx = (LCII_context_t*)packet->data.rtr.send_ctx;
  if (ctx->data.lbuffer.segment == LCI_SEGMENT_ALL) {
    LCI_DBG_Assert(LCII_comp_attr_get_dereg(ctx->comp_attr) == 1, "\n");
    LCI_memory_register(ep->device, ctx->data.lbuffer.address,
                        ctx->data.lbuffer.length, &ctx->data.lbuffer.segment);
  } else {
    LCI_DBG_Assert(LCII_comp_attr_get_dereg(ctx->comp_attr) == 0, "\n");
  }
  LCIS_post_putImm_bq(
      ep->bq_p, ep->bq_spinlock_p, ep->device->endpoint_progress.endpoint,
      ctx->rank, ctx->data.lbuffer.address, ctx->data.lbuffer.length,
      ctx->data.lbuffer.segment->mr, packet->data.rtr.remote_addr_base,
      packet->data.rtr.remote_addr_offset, packet->data.rtr.rkey,
      LCII_MAKE_PROTO(ep->gid, LCI_MSG_RDMA_LONG,
                      packet->data.rtr.recv_ctx_key),
      ctx);
  LCII_free_packet(packet);
}

static inline void LCII_handle_1sided_writeImm(LCI_endpoint_t ep,
                                               uint64_t ctx_key)
{
  LCII_context_t* ctx =
      (LCII_context_t*)LCM_archive_remove(ep->ctx_archive_p, ctx_key);
  LCI_DBG_Assert(ctx->data_type == LCI_LONG,
                 "Didn't get the right context! This might imply some bugs in "
                 "the LCM_archive_t.\n");
  // putl has been completed locally. Need to process completion.
  LCII_PCOUNTER_ADD(net_recv_comp, ctx->data.lbuffer.length);
  lc_ce_dispatch(ctx);
}

static inline void LCII_handle_iovec_rts(LCI_endpoint_t ep,
                                         LCII_packet_t* packet,
                                         uint32_t src_rank, uint16_t tag)
{
  LCII_context_t* rdv_ctx = LCIU_malloc(sizeof(LCII_context_t));
  rdv_ctx->data.iovec.count = packet->data.rts.count;
  rdv_ctx->data.iovec.piggy_back.length = packet->data.rts.piggy_back_size;
  rdv_ctx->data.iovec.piggy_back.address =
      LCIU_malloc(packet->data.rts.piggy_back_size);
  memcpy(rdv_ctx->data.iovec.piggy_back.address,
         (void*)&packet->data.rts.size_p[packet->data.rts.count],
         packet->data.rts.piggy_back_size);
  rdv_ctx->data.iovec.lbuffers =
      LCIU_malloc(sizeof(LCI_lbuffer_t) * packet->data.rts.count);
  for (int i = 0; i < packet->data.rts.count; ++i) {
    LCI_lbuffer_alloc(ep->device, packet->data.rts.size_p[i],
                      &rdv_ctx->data.iovec.lbuffers[i]);
  }
  rdv_ctx->data_type = LCI_IOVEC;
  rdv_ctx->rank = src_rank;
  rdv_ctx->tag = tag;
  rdv_ctx->user_context = NULL;
  LCII_initilize_comp_attr(rdv_ctx->comp_attr);
  LCII_comp_attr_set_msg_type(rdv_ctx->comp_attr, LCI_MSG_NONE);
  LCII_comp_attr_set_comp_type(rdv_ctx->comp_attr, ep->msg_comp_type);
  rdv_ctx->completion = ep->default_comp;

  LCII_context_t* rtr_ctx = LCIU_malloc(sizeof(LCII_context_t));
  rtr_ctx->data.mbuffer.address = &(packet->data);
  LCII_initilize_comp_attr(rtr_ctx->comp_attr);
  LCII_comp_attr_set_msg_type(rtr_ctx->comp_attr, LCI_MSG_RTR);
  LCII_comp_attr_set_free_packet(rtr_ctx->comp_attr, 1);

  // reuse the rts packet as rtr packet
  packet->context.poolid = -1;
  packet->data.rtr.recv_ctx = (uintptr_t)rdv_ctx;
  for (int i = 0; i < rdv_ctx->data.iovec.count; ++i) {
    packet->data.rtr.remote_iovecs_p[i].remote_addr_base =
        (uintptr_t)rdv_ctx->data.iovec.lbuffers[i].segment->mr.address;
    packet->data.rtr.remote_iovecs_p[i].remote_addr_offset =
        (uintptr_t)rdv_ctx->data.iovec.lbuffers[i].address -
        packet->data.rtr.remote_iovecs_p[i].remote_addr_base;
    packet->data.rtr.remote_iovecs_p[i].rkey =
        LCIS_rma_rkey(rdv_ctx->data.iovec.lbuffers[i].segment->mr);
  }

  LCI_DBG_Log(LCI_LOG_TRACE, "rdv",
              "send rtr: type %d sctx %p count %d rctx %p\n",
              packet->data.rtr.msg_type, (void*)packet->data.rtr.send_ctx,
              rdv_ctx->data.iovec.count, (void*)packet->data.rtr.recv_ctx);
  size_t length =
      (uintptr_t)&packet->data.rtr.remote_iovecs_p[rdv_ctx->data.iovec.count] -
      (uintptr_t)packet->data.address;
  LCIS_post_send_bq(ep->bq_p, ep->bq_spinlock_p,
                    ep->device->endpoint_progress.endpoint, rdv_ctx->rank,
                    packet->data.address, length, ep->device->heap.segment->mr,
                    LCII_MAKE_PROTO(ep->gid, LCI_MSG_RTR, 0), rtr_ctx);
}

static inline void LCII_handle_iovec_rtr(LCI_endpoint_t ep,
                                         LCII_packet_t* packet)
{
  LCII_context_t* ctx = (LCII_context_t*)packet->data.rtr.send_ctx;
  LCII_extended_context_t* ectx = LCIU_malloc(sizeof(LCII_extended_context_t));
  LCII_initilize_comp_attr(ectx->comp_attr);
  LCII_comp_attr_set_extended(ectx->comp_attr, 1);
#ifdef LCI_ENABLE_MULTITHREAD_PROGRESS
  atomic_init(&ectx->signal_count, 0);
#else
  ectx->signal_count = 0;
#endif
  ectx->signal_expected = ctx->data.iovec.count;
  ectx->context = ctx;
  ectx->ep = ep;
  ectx->recv_ctx = packet->data.rtr.recv_ctx;
  LCII_comp_attr_set_dereg(
      ectx->comp_attr, ctx->data.iovec.lbuffers[0].segment == LCI_SEGMENT_ALL);
  for (int i = 0; i < ctx->data.iovec.count; ++i) {
    if (ctx->data.iovec.lbuffers[i].segment == LCI_SEGMENT_ALL) {
      LCI_DBG_Assert(LCII_comp_attr_get_dereg(ectx->comp_attr) == 1, "\n");
      LCI_memory_register(ep->device, ctx->data.iovec.lbuffers[i].address,
                          ctx->data.iovec.lbuffers[i].length,
                          &ctx->data.iovec.lbuffers[i].segment);
    } else {
      LCI_DBG_Assert(LCII_comp_attr_get_dereg(ectx->comp_attr) == 0, "\n");
    }
    LCIS_post_put_bq(ep->bq_p, ep->bq_spinlock_p,
                     ep->device->endpoint_progress.endpoint, ctx->rank,
                     ctx->data.iovec.lbuffers[i].address,
                     ctx->data.iovec.lbuffers[i].length,
                     ctx->data.iovec.lbuffers[i].segment->mr,
                     packet->data.rtr.remote_iovecs_p[i].remote_addr_base,
                     packet->data.rtr.remote_iovecs_p[i].remote_addr_offset,
                     packet->data.rtr.remote_iovecs_p[i].rkey, ectx);
  }
  LCII_free_packet(packet);
}

static inline void LCII_handle_iovec_put_comp(LCII_extended_context_t* ectx)
{
#ifdef LCI_ENABLE_MULTITHREAD_PROGRESS
  int signal_count = atomic_fetch_add_explicit(&ectx->signal_count, 1,
                                               LCIU_memory_order_relaxed) +
                     1;
#else
  int signal_count = ++ectx->signal_count;
#endif
  // Assuming ibv_post_send will act as a full memory barrier, signal_expected
  // doesn't need to be an atomic variable.
  if (signal_count < ectx->signal_expected) {
    return;
  }
  LCI_DBG_Assert(signal_count == ectx->signal_expected, "Unexpected signal!\n");
  LCII_context_t* ctx = ectx->context;
  LCI_DBG_Log(LCI_LOG_TRACE, "rdv", "send FIN: rctx %p\n",
              (void*)ectx->recv_ctx);
  LCIS_post_sends_bq(ectx->ep->bq_p, ectx->ep->bq_spinlock_p,
                     ectx->ep->device->endpoint_progress.endpoint, ctx->rank,
                     &ectx->recv_ctx, sizeof(ectx->recv_ctx),
                     LCII_MAKE_PROTO(ectx->ep->gid, LCI_MSG_FIN, 0));
  if (LCII_comp_attr_get_dereg(ectx->comp_attr) == 1) {
    for (int i = 0; i < ctx->data.iovec.count; ++i) {
      LCI_memory_deregister(&ctx->data.iovec.lbuffers[i].segment);
      ctx->data.iovec.lbuffers[i].segment = LCI_SEGMENT_ALL;
    }
  }
  LCIU_free(ectx);
  lc_ce_dispatch(ctx);
}

static inline void LCII_handle_iovec_recv_FIN(LCII_packet_t* packet)
{
  LCII_context_t* ctx;
  memcpy(&ctx, packet->data.address, sizeof(ctx));
  LCI_DBG_Log(LCI_LOG_TRACE, "rdv", "recv FIN: rctx %p\n", ctx);
  LCI_DBG_Assert(ctx->data_type == LCI_IOVEC,
                 "Didn't get the right context (%p type=%d)!.\n", ctx,
                 ctx->data_type);
  // putva has been completed locally. Need to process completion.
  LCII_PCOUNTER_ADD(net_recv_comp, sizeof(ctx));
  for (int i = 0; i < ctx->data.iovec.count; ++i)
    LCII_PCOUNTER_ADD(net_recv_comp, ctx->data.iovec.lbuffers[i].length);
  LCII_free_packet(packet);
  lc_ce_dispatch(ctx);
}

#endif  // LCI_RENDEZVOUS_H
