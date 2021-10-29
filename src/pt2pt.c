#include "lci.h"
#include "lcii.h"

LCI_error_t LCI_sends(LCI_endpoint_t ep, LCI_short_t src, int rank, LCI_tag_t tag)
{
  LCM_DBG_Assert(tag <= LCI_MAX_TAG, "tag %d is too large (maximum: %d)\n", tag, LCI_MAX_TAG);
  return lc_server_sends(ep->device->server, rank, &src, sizeof(LCI_short_t),
                         LCII_MAKE_PROTO(ep->gid, LCI_MSG_SHORT, tag));
}

LCI_error_t LCI_sendm(LCI_endpoint_t ep, LCI_mbuffer_t buffer, int rank,
                      LCI_tag_t tag)
{
  LCM_DBG_Assert(tag <= LCI_MAX_TAG, "tag %d is too large (maximum: %d)\n", tag, LCI_MAX_TAG);
  lc_packet* packet = lc_pool_get_nb(ep->pkpool);
  if (packet == NULL)
    // no packet is available
    return LCI_ERR_RETRY;
  packet->context.poolid = (buffer.length > LCI_PACKET_RETURN_THRESHOLD) ?
                           lc_pool_get_local(ep->pkpool) : -1;
  memcpy(packet->data.address, buffer.address, buffer.length);

  LCII_context_t *ctx = LCIU_malloc(sizeof(LCII_context_t));
  ctx->data.mbuffer.address = (void*) packet->data.address;
  ctx->comp_type = LCI_COMPLETION_FREE;

  LCI_error_t ret = lc_server_send(ep->device->server, rank, packet->data.address, buffer.length,
                                   *(ep->device->heap.segment),
                                   LCII_MAKE_PROTO(ep->gid, LCI_MSG_MEDIUM, tag), ctx);
  if (ret == LCI_ERR_RETRY) {
    LCII_free_packet(packet);
    LCIU_free(ctx);
  }
  return ret;
}

LCI_error_t LCI_sendmn(LCI_endpoint_t ep, LCI_mbuffer_t buffer, int rank,
                       LCI_tag_t tag)
{
  LCM_DBG_Assert(tag <= LCI_MAX_TAG, "tag %d is too large (maximum: %d)\n", tag, LCI_MAX_TAG);
  lc_packet* packet = LCII_mbuffer2packet(buffer);
  packet->context.poolid = (buffer.length > LCI_PACKET_RETURN_THRESHOLD) ?
                           lc_pool_get_local(ep->pkpool) : -1;

  LCII_context_t *ctx = LCIU_malloc(sizeof(LCII_context_t));
  ctx->data.mbuffer.address = (void*) packet->data.address;
  ctx->comp_type = LCI_COMPLETION_FREE;

  LCI_error_t ret = lc_server_send(ep->device->server, rank, packet->data.address, buffer.length,
                                   *(ep->device->heap.segment),
                                   LCII_MAKE_PROTO(ep->gid, LCI_MSG_MEDIUM, tag), ctx);
  if (ret == LCI_ERR_RETRY) {
    LCIU_free(ctx);
  }
  return ret;
}

LCI_error_t LCI_sendl(LCI_endpoint_t ep, LCI_lbuffer_t buffer, uint32_t rank,
                      LCI_tag_t tag, LCI_comp_t completion, void* user_context)
{
  LCM_DBG_Assert(tag <= LCI_MAX_TAG, "tag %d is too large (maximum: %d)\n", tag, LCI_MAX_TAG);
  lc_packet* packet = lc_pool_get_nb(ep->pkpool);
  if (packet == NULL)
    // no packet is available
    return LCI_ERR_RETRY;
  packet->context.poolid = -1;

  LCII_context_t *rts_ctx = LCIU_malloc(sizeof(LCII_context_t));
  rts_ctx->data.mbuffer.address = (void*) &(packet->data);
  rts_ctx->comp_type = LCI_COMPLETION_FREE;

  LCII_context_t *rdv_ctx = LCIU_malloc(sizeof(LCII_context_t));
  rdv_ctx->data.lbuffer = buffer;
  rdv_ctx->data_type = LCI_LONG;
  rdv_ctx->rank = rank;
  rdv_ctx->tag = tag;
  rdv_ctx->user_context = user_context;
  rdv_ctx->comp_type = ep->cmd_comp_type;
  rdv_ctx->completion = completion;

  packet->data.rts.msg_type = LCI_MSG_LONG;
  packet->data.rts.send_ctx = (uintptr_t) rdv_ctx;
  packet->data.rts.size = buffer.length;

  LCI_error_t ret = lc_server_send(ep->device->server, rank, packet->data.address,
                                   sizeof(struct packet_rts), *(ep->device->heap.segment),
                                   LCII_MAKE_PROTO(ep->gid, LCI_MSG_RTS, tag), rts_ctx);
  if (ret == LCI_ERR_RETRY) {
    LCII_free_packet(packet);
    LCIU_free(rts_ctx);
    LCIU_free(rdv_ctx);
  }
  return ret;
}

LCI_error_t LCI_recvs(LCI_endpoint_t ep, int rank, LCI_tag_t tag,
                      LCI_comp_t completion, void* user_context)
{
  LCM_DBG_Assert(tag <= LCI_MAX_TAG, "tag %d is too large (maximum: %d)\n", tag, LCI_MAX_TAG);
  LCII_context_t *ctx = LCIU_malloc(sizeof(LCII_context_t));
  ctx->data_type = LCI_IMMEDIATE;
  ctx->rank = rank;
  ctx->tag = tag;
  ctx->user_context = user_context;
  ctx->comp_type = ep->msg_comp_type;
  ctx->completion = completion;

  lc_key key = LCII_make_key(ep, rank, tag, LCI_MSG_SHORT);
  lc_value value = (lc_value)ctx;
  if (!lc_hash_insert(ep->mt, key, &value, CLIENT)) {
    lc_packet* packet = (lc_packet*) value;
    ctx->rank = packet->context.src_rank;
    memcpy(&(ctx->data.immediate), packet->data.address, LCI_SHORT_SIZE);
    LCII_free_packet(packet);
    lc_ce_dispatch(ctx);
  }
  return LCI_OK;
}

LCI_error_t LCI_recvm(LCI_endpoint_t ep, LCI_mbuffer_t buffer, int rank,
                      LCI_tag_t tag, LCI_comp_t completion, void* user_context)
{
  LCM_DBG_Assert(tag <= LCI_MAX_TAG, "tag %d is too large (maximum: %d)\n", tag, LCI_MAX_TAG);
  LCII_context_t *ctx = LCIU_malloc(sizeof(LCII_context_t));
  ctx->data.mbuffer = buffer;
  ctx->data_type = LCI_MEDIUM;
  ctx->rank = rank;
  ctx->tag = tag;
  ctx->user_context = user_context;
  ctx->comp_type = ep->msg_comp_type;
  ctx->completion = completion;

  lc_key key = LCII_make_key(ep, rank, tag, LCI_MSG_MEDIUM);
  lc_value value = (lc_value)ctx;
  if (!lc_hash_insert(ep->mt, key, &value, CLIENT)) {
    lc_packet* packet = (lc_packet*) value;
    ctx->rank = packet->context.src_rank;
    ctx->data.mbuffer.length = packet->context.length;
    // copy to user provided buffer
    memcpy(ctx->data.mbuffer.address, packet->data.address, ctx->data.mbuffer.length);
    LCII_free_packet(packet);
    lc_ce_dispatch(ctx);
  }
  return LCI_OK;
}

LCI_error_t LCI_recvmn(LCI_endpoint_t ep, int rank, LCI_tag_t tag,
                       LCI_comp_t completion, void* user_context)
{
  LCM_DBG_Assert(tag <= LCI_MAX_TAG, "tag %d is too large (maximum: %d)\n", tag, LCI_MAX_TAG);
  LCII_context_t *ctx = LCIU_malloc(sizeof(LCII_context_t));
  ctx->data.mbuffer.address = NULL;
  ctx->data_type = LCI_MEDIUM;
  ctx->rank = rank;
  ctx->tag = tag;
  ctx->user_context = user_context;
  ctx->comp_type = ep->msg_comp_type;
  ctx->completion = completion;

  lc_key key = LCII_make_key(ep, rank, tag, LCI_MSG_MEDIUM);
  lc_value value = (lc_value)ctx;
  if (!lc_hash_insert(ep->mt, key, &value, CLIENT)) {
    lc_packet* packet = (lc_packet*) value;
    ctx->rank = packet->context.src_rank;
    ctx->data.mbuffer.length = packet->context.length;
    // use LCI packet
    ctx->data.mbuffer.address = packet->data.address;
    lc_ce_dispatch(ctx);
  }
  return LCI_OK;
}

LCI_error_t LCI_recvl(LCI_endpoint_t ep, LCI_lbuffer_t buffer, uint32_t rank,
                      LCI_tag_t tag, LCI_comp_t completion, void* user_context)
{
  LCM_DBG_Assert(tag <= LCI_MAX_TAG, "tag %d is too large (maximum: %d)\n", tag, LCI_MAX_TAG);
  LCII_context_t *rdv_ctx = LCIU_malloc(sizeof(LCII_context_t));
  rdv_ctx->data.lbuffer = buffer;
  rdv_ctx->data_type = LCI_LONG;
  rdv_ctx->rank = rank;
  rdv_ctx->tag = tag;
  rdv_ctx->user_context = user_context;
  rdv_ctx->comp_type = ep->msg_comp_type;
  rdv_ctx->completion = completion;

  lc_key key = LCII_make_key(ep, rank, tag, LCI_MSG_LONG);
  lc_value value = (lc_value)rdv_ctx;
  if (!lc_hash_insert(ep->mt, key, &value, CLIENT)) {
    lc_packet* p = (lc_packet*) value;
    LCII_handle_2sided_rts(ep, p, rdv_ctx);
  }
  return LCI_OK;
}
