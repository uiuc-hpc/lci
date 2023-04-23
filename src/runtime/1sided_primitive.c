#include "lci.h"
#include "runtime/lcii.h"

LCI_error_t LCI_puts(LCI_endpoint_t ep, LCI_short_t src, int rank,
                     LCI_tag_t tag, uintptr_t remote_completion)
{
  LCM_DBG_Assert(tag <= LCI_MAX_TAG, "tag %d is too large (maximum: %d)\n", tag,
                 LCI_MAX_TAG);
  LCM_DBG_Assert(remote_completion == LCI_DEFAULT_COMP_REMOTE,
                 "Only support default remote completion "
                 "(set by LCI_plist_set_default_comp, "
                 "the default value is LCI_UR_CQ)\n");
  LCI_error_t ret = LCIS_post_sends(
      ep->device->endpoint_worker.endpoint, rank, &src, sizeof(LCI_short_t),
      LCII_MAKE_PROTO(ep->gid, LCI_MSG_RDMA_SHORT, tag));
  if (ret == LCI_OK) {
    LCII_PCOUNTERS_WRAPPER(
        LCII_pcounters[LCIU_get_thread_id()].send_lci_succeeded++);
  } else {
    LCII_PCOUNTERS_WRAPPER(
        LCII_pcounters[LCIU_get_thread_id()].send_lci_failed_backend++);
  }
  LCM_DBG_Log(LCM_LOG_DEBUG, "comm",
              "LCI_puts(ep %p, rank %d, tag %u, remote_completion %p) -> %d\n",
              ep, rank, tag, (void*)remote_completion, ret);
  return ret;
}

LCI_error_t LCI_putm(LCI_endpoint_t ep, LCI_mbuffer_t mbuffer, int rank,
                     LCI_tag_t tag, LCI_lbuffer_t remote_buffer,
                     uintptr_t remote_completion)
{
  //  LC_POOL_GET_OR_RETN(ep->pkpool, p);
  //  lc_pk_init(ep, (size > 1024) ? lc_pool_get_local(ep->pkpool) : -1,
  //  LC_PROTO_DATA, p); struct lc_rep* rep = &(ep->rep[rank]);
  //  memcpy(p->data.buffer, src, size);
  //  LCIS_post_put(ep->server, rep->handle, rep->base, offset, rep->rkey, size,
  //  LCII_MAKE_PROTO(ep->gid, LC_PROTO_LONG, meta), p);
  return LCI_ERR_FEATURE_NA;
}

LCI_error_t LCI_putma(LCI_endpoint_t ep, LCI_mbuffer_t buffer, int rank,
                      LCI_tag_t tag, uintptr_t remote_completion)
{
  LCM_DBG_Assert(tag <= LCI_MAX_TAG, "tag %d is too large (maximum: %d)\n", tag,
                 LCI_MAX_TAG);
  LCM_DBG_Assert(buffer.length <= LCI_MEDIUM_SIZE,
                 "buffer is too large %lu (maximum: %d)\n", buffer.length,
                 LCI_MEDIUM_SIZE);
  LCM_DBG_Assert(remote_completion == LCI_DEFAULT_COMP_REMOTE,
                 "Only support default remote completion "
                 "(set by LCI_plist_set_default_comp, "
                 "the default value is LCI_UR_CQ)\n");
  LCI_error_t ret = LCI_OK;
  if (buffer.length <= LCI_SHORT_SIZE) {
    /* if data is this short, we will be able to inline it
     * no reason to get a packet, allocate a ctx, etc */
    ret = LCIS_post_sends(ep->device->endpoint_worker.endpoint, rank,
                          buffer.address, buffer.length,
                          LCII_MAKE_PROTO(ep->gid, LCI_MSG_RDMA_MEDIUM, tag));
  } else {
    LCII_packet_t* packet = LCII_alloc_packet_nb(ep->pkpool);
    if (packet == NULL) {
      // no packet is available
      LCII_PCOUNTERS_WRAPPER(
          LCII_pcounters[LCIU_get_thread_id()].send_lci_failed_packet++);
      return LCI_ERR_RETRY;
    }
    packet->context.poolid = (buffer.length > LCI_PACKET_RETURN_THRESHOLD)
                                 ? lc_pool_get_local(ep->pkpool)
                                 : -1;
    memcpy(packet->data.address, buffer.address, buffer.length);

    LCII_context_t* ctx = LCIU_malloc(sizeof(LCII_context_t));
    ctx->data.mbuffer.address = (void*)packet->data.address;
    LCII_PCOUNTERS_WRAPPER(ctx->timer = LCII_ucs_get_time());
    LCII_initilize_comp_attr(ctx->comp_attr);
    LCII_comp_attr_set_msg_type(ctx->comp_attr, LCI_MSG_RDMA_MEDIUM);
    LCII_comp_attr_set_free_packet(ctx->comp_attr, 1);

    ret = LCIS_post_send(
        ep->device->endpoint_worker.endpoint, rank, packet->data.address,
        buffer.length, ep->device->heap.segment->mr,
        LCII_MAKE_PROTO(ep->gid, LCI_MSG_RDMA_MEDIUM, tag), ctx);
    if (ret == LCI_ERR_RETRY) {
      LCII_free_packet(packet);
      LCIU_free(ctx);
    }
  }
  if (ret == LCI_OK) {
    LCII_PCOUNTERS_WRAPPER(
        LCII_pcounters[LCIU_get_thread_id()].send_lci_succeeded++);
  } else {
    LCII_PCOUNTERS_WRAPPER(
        LCII_pcounters[LCIU_get_thread_id()].send_lci_failed_backend++);
  }
  LCM_DBG_Log(LCM_LOG_DEBUG, "comm",
              "LCI_putm(ep %p, buffer {%p, %lu}, rank %d, tag %u, "
              "remote_completion %p) -> %d\n",
              ep, buffer.address, buffer.length, rank, tag,
              (void*)remote_completion, ret);
  return ret;
}

LCI_error_t LCI_putmna(LCI_endpoint_t ep, LCI_mbuffer_t buffer, int rank,
                       LCI_tag_t tag, uintptr_t remote_completion)
{
  LCM_DBG_Assert(tag <= LCI_MAX_TAG, "tag %d is too large (maximum: %d)\n", tag,
                 LCI_MAX_TAG);
  LCM_DBG_Assert(buffer.length <= LCI_MEDIUM_SIZE,
                 "buffer is too large %lu (maximum: %d)\n", buffer.length,
                 LCI_MEDIUM_SIZE);
  LCM_DBG_Assert(remote_completion == LCI_DEFAULT_COMP_REMOTE,
                 "Only support default remote completion "
                 "(set by LCI_plist_set_default_comp, "
                 "the default value is LCI_UR_CQ)\n");
  LCII_packet_t* packet = LCII_mbuffer2packet(buffer);
  packet->context.poolid = (buffer.length > LCI_PACKET_RETURN_THRESHOLD)
                               ? lc_pool_get_local(ep->pkpool)
                               : -1;

  LCII_context_t* ctx = LCIU_malloc(sizeof(LCII_context_t));
  LCII_PCOUNTERS_WRAPPER(ctx->timer = LCII_ucs_get_time());
  ctx->data.mbuffer.address = (void*)packet->data.address;
  LCII_initilize_comp_attr(ctx->comp_attr);
  LCII_comp_attr_set_msg_type(ctx->comp_attr, LCI_MSG_RDMA_MEDIUM);
  LCII_comp_attr_set_free_packet(ctx->comp_attr, 1);

  LCI_error_t ret = LCIS_post_send(
      ep->device->endpoint_worker.endpoint, rank, packet->data.address,
      buffer.length, ep->device->heap.segment->mr,
      LCII_MAKE_PROTO(ep->gid, LCI_MSG_RDMA_MEDIUM, tag), ctx);
  if (ret == LCI_ERR_RETRY) {
    LCIU_free(ctx);
  }
  if (ret == LCI_OK) {
    LCII_PCOUNTERS_WRAPPER(
        LCII_pcounters[LCIU_get_thread_id()].send_lci_succeeded++);
  } else {
    LCII_PCOUNTERS_WRAPPER(
        LCII_pcounters[LCIU_get_thread_id()].send_lci_failed_backend++);
  }
  LCM_DBG_Log(LCM_LOG_DEBUG, "comm",
              "LCI_putmna(ep %p, buffer {%p, %lu}, rank %d, tag %u, "
              "remote_completion %p) -> %d\n",
              ep, buffer.address, buffer.length, rank, tag,
              (void*)remote_completion, ret);
  return ret;
}

LCI_error_t LCI_putl(LCI_endpoint_t ep, LCI_lbuffer_t local_buffer,
                     LCI_comp_t local_completion, int rank, LCI_tag_t tag,
                     LCI_lbuffer_t remote_buffer, uintptr_t remote_completion)
{
  return LCI_ERR_FEATURE_NA;
}

LCI_error_t LCI_putla(LCI_endpoint_t ep, LCI_lbuffer_t buffer,
                      LCI_comp_t completion, int rank, LCI_tag_t tag,
                      uintptr_t remote_completion, void* user_context)
{
  LCM_DBG_Assert(tag <= LCI_MAX_TAG, "tag %d is too large (maximum: %d)\n", tag,
                 LCI_MAX_TAG);
  LCM_DBG_Assert(remote_completion == LCI_DEFAULT_COMP_REMOTE,
                 "Only support default remote completion "
                 "(set by LCI_plist_set_default_comp, "
                 "the default value is LCI_UR_CQ)\n");
  if (!LCII_bq_is_empty(ep->bq_p)) {
    LCII_PCOUNTERS_WRAPPER(
        LCII_pcounters[LCIU_get_thread_id()].send_lci_failed_bq++);
    return LCI_ERR_RETRY;
  }
  LCII_packet_t* packet = LCII_alloc_packet_nb(ep->pkpool);
  if (packet == NULL) {
    // no packet is available
    LCII_PCOUNTERS_WRAPPER(
        LCII_pcounters[LCIU_get_thread_id()].send_lci_failed_packet++);
    return LCI_ERR_RETRY;
  }
  packet->context.poolid = -1;

  LCII_context_t* rts_ctx = LCIU_malloc(sizeof(LCII_context_t));
  rts_ctx->data.mbuffer.address = (void*)packet->data.address;
  LCII_initilize_comp_attr(rts_ctx->comp_attr);
  LCII_comp_attr_set_msg_type(rts_ctx->comp_attr, LCI_MSG_RTS);
  LCII_comp_attr_set_free_packet(rts_ctx->comp_attr, 1);

  LCII_context_t* rdv_ctx = LCIU_malloc(sizeof(LCII_context_t));
  rdv_ctx->data.lbuffer = buffer;
  rdv_ctx->data_type = LCI_LONG;
  rdv_ctx->rank = rank;
  rdv_ctx->tag = tag;
  rdv_ctx->user_context = user_context;
  LCII_initilize_comp_attr(rdv_ctx->comp_attr);
  LCII_comp_attr_set_msg_type(rdv_ctx->comp_attr, LCI_MSG_RDMA_LONG);
  LCII_comp_attr_set_comp_type(rdv_ctx->comp_attr, ep->cmd_comp_type);
  LCII_comp_attr_set_dereg(rdv_ctx->comp_attr,
                           buffer.segment == LCI_SEGMENT_ALL);
  rdv_ctx->completion = completion;

  packet->data.rts.msg_type = LCI_MSG_RDMA_LONG;
  packet->data.rts.send_ctx = (uintptr_t)rdv_ctx;
  packet->data.rts.size = buffer.length;

  LCM_DBG_Log(LCM_LOG_DEBUG, "rdv", "send rts: type %d sctx %p size %lu\n",
              packet->data.rts.msg_type, (void*)packet->data.rts.send_ctx,
              packet->data.rts.size);
  LCI_error_t ret = LCIS_post_send(
      ep->device->endpoint_worker.endpoint, rank, packet->data.address,
      sizeof(struct LCII_packet_rts_t), ep->device->heap.segment->mr,
      LCII_MAKE_PROTO(ep->gid, LCI_MSG_RTS, tag), rts_ctx);
  if (ret == LCI_ERR_RETRY) {
    LCII_free_packet(packet);
    LCIU_free(rts_ctx);
    LCIU_free(rdv_ctx);
  }
  if (ret == LCI_OK) {
    LCII_PCOUNTERS_WRAPPER(
        LCII_pcounters[LCIU_get_thread_id()].send_lci_succeeded++);
  } else {
    LCII_PCOUNTERS_WRAPPER(
        LCII_pcounters[LCIU_get_thread_id()].send_lci_failed_backend++);
  }
  LCM_DBG_Log(LCM_LOG_DEBUG, "comm",
              "LCI_putla(ep %p, buffer {%p, %lu, %p}, completion %p, rank %d, "
              "tag %u, remote_completion %p, user_context %p) -> %d\n",
              ep, buffer.address, buffer.length, buffer.segment, completion,
              rank, tag, (void*)remote_completion, user_context, ret);
  return ret;
}

LCI_error_t LCI_putva(LCI_endpoint_t ep, LCI_iovec_t iovec,
                      LCI_comp_t completion, int rank, LCI_tag_t tag,
                      uintptr_t remote_completion, void* user_context)
{
  LCM_DBG_Assert(tag <= LCI_MAX_TAG, "tag %d is too large (maximum: %d)\n", tag,
                 LCI_MAX_TAG);
  LCM_DBG_Assert(remote_completion == LCI_DEFAULT_COMP_REMOTE,
                 "Only support default remote completion "
                 "(set by LCI_plist_set_default_comp, "
                 "the default value is LCI_UR_CQ)\n");
  LCM_DBG_Assert(iovec.count > 0, "iovec.count = %d!\n", iovec.count);
  LCM_DBG_Assert(iovec.count <= LCI_IOVEC_SIZE,
                 "iovec.count = %d > "
                 "LCI_IOVEC_SIZE %d!\n",
                 iovec.count, LCI_IOVEC_SIZE);
  LCM_DBG_Assert(
      iovec.piggy_back.length <= LCI_get_iovec_piggy_back_size(iovec.count),
      "iovec's piggy back is too large! (%lu > %lu)\n", iovec.piggy_back.length,
      LCI_get_iovec_piggy_back_size(iovec.count));
  for (int i = 0; i < iovec.count; ++i) {
    LCM_DBG_Assert(
        (iovec.lbuffers[0].segment == LCI_SEGMENT_ALL &&
         iovec.lbuffers[i].segment == LCI_SEGMENT_ALL) ||
            (iovec.lbuffers[0].segment != LCI_SEGMENT_ALL &&
             iovec.lbuffers[i].segment != LCI_SEGMENT_ALL),
        "We currently require either all lbuffers to be registers or "
        "all of them are LCI_SEGMENT_ALL\n");
    LCM_DBG_Assert(iovec.lbuffers[i].length > 0, "Invalid lbuffer length\n");
  }
  if (!LCII_bq_is_empty(ep->bq_p)) {
    LCII_PCOUNTERS_WRAPPER(
        LCII_pcounters[LCIU_get_thread_id()].send_lci_failed_bq++);
    return LCI_ERR_RETRY;
  }
  LCII_packet_t* packet = LCII_alloc_packet_nb(ep->pkpool);
  if (packet == NULL) {
    // no packet is available
    LCII_PCOUNTERS_WRAPPER(
        LCII_pcounters[LCIU_get_thread_id()].send_lci_failed_packet++);
    return LCI_ERR_RETRY;
  }
  packet->context.poolid =
      (iovec.piggy_back.length > LCI_PACKET_RETURN_THRESHOLD)
          ? lc_pool_get_local(ep->pkpool)
          : -1;

  LCII_context_t* rts_ctx = LCIU_malloc(sizeof(LCII_context_t));
  rts_ctx->data.mbuffer.address = (void*)packet->data.address;
  LCII_initilize_comp_attr(rts_ctx->comp_attr);
  LCII_comp_attr_set_msg_type(rts_ctx->comp_attr, LCI_MSG_RTS);
  LCII_comp_attr_set_free_packet(rts_ctx->comp_attr, 1);

  LCII_context_t* rdv_ctx = LCIU_malloc(sizeof(LCII_context_t));
  LCII_PCOUNTERS_WRAPPER(rdv_ctx->timer = LCII_ucs_get_time());
  rdv_ctx->data.iovec = iovec;
  rdv_ctx->data_type = LCI_IOVEC;
  rdv_ctx->rank = rank;
  rdv_ctx->tag = tag;
  rdv_ctx->user_context = user_context;
  LCII_initilize_comp_attr(rdv_ctx->comp_attr);
  LCII_comp_attr_set_msg_type(rdv_ctx->comp_attr, LCI_MSG_IOVEC);
  LCII_comp_attr_set_comp_type(rdv_ctx->comp_attr, ep->cmd_comp_type);
  rdv_ctx->completion = completion;

  packet->data.rts.msg_type = LCI_MSG_IOVEC;
  packet->data.rts.send_ctx = (uintptr_t)rdv_ctx;
  packet->data.rts.count = iovec.count;
  packet->data.rts.piggy_back_size = iovec.piggy_back.length;
  for (int i = 0; i < iovec.count; ++i) {
    packet->data.rts.size_p[i] = iovec.lbuffers[i].length;
  }
  memcpy((void*)&packet->data.rts.size_p[iovec.count], iovec.piggy_back.address,
         iovec.piggy_back.length);

  LCM_DBG_Log(LCM_LOG_DEBUG, "rdv",
              "send rts: type %d sctx %p count %d "
              "piggy_back_size %lu\n",
              packet->data.rts.msg_type, (void*)packet->data.rts.send_ctx,
              packet->data.rts.count, packet->data.rts.piggy_back_size);
  size_t length = (uintptr_t)&packet->data.rts.size_p[iovec.count] -
                  (uintptr_t)packet->data.address + iovec.piggy_back.length;
  LCI_error_t ret =
      LCIS_post_send(ep->device->endpoint_worker.endpoint, rank,
                     packet->data.address, length, ep->device->heap.segment->mr,
                     LCII_MAKE_PROTO(ep->gid, LCI_MSG_RTS, tag), rts_ctx);
  if (ret == LCI_ERR_RETRY) {
    LCII_free_packet(packet);
    LCIU_free(rts_ctx);
    LCIU_free(rdv_ctx);
  }
  if (ret == LCI_OK) {
    LCII_PCOUNTERS_WRAPPER(
        LCII_pcounters[LCIU_get_thread_id()].send_lci_succeeded++);
  } else {
    LCII_PCOUNTERS_WRAPPER(
        LCII_pcounters[LCIU_get_thread_id()].send_lci_failed_backend++);
  }
  LCM_DBG_Log(LCM_LOG_DEBUG, "comm",
              "LCI_putva(ep %p, iovec {{%p, %lu}, %d, %p}, completion %p, rank "
              "%d, tag %u, remote_completion %p, user_context %p) -> %d\n",
              ep, iovec.piggy_back.address, iovec.piggy_back.length,
              iovec.count, iovec.lbuffers, completion, rank, tag,
              (void*)remote_completion, user_context, ret);
  return ret;
}

size_t LCI_get_iovec_piggy_back_size(int count)
{
  LCM_DBG_Assert(LCI_MEDIUM_SIZE > 0,
                 "LCI_MEDIUM_SIZE <=0! You should run "
                 "LCI_initialize() before calling this function\n");
  return LCI_MEDIUM_SIZE - sizeof(struct LCII_packet_rts_t) -
         sizeof(size_t) * count;
}
