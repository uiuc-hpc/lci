#include "lci.h"
#include "lcii.h"

LCI_error_t LCI_puts(LCI_endpoint_t ep, LCI_short_t src, int rank,
                     LCI_tag_t tag, uintptr_t remote_completion)
{
  LCM_DBG_Assert(remote_completion == LCI_UR_CQ_REMOTE, "Only support default remote completion (dynamic put to remote LCI_UR_CQ)\n");
  return lc_server_sends(ep->device->server, rank, &src, sizeof(LCI_short_t),
                         LCII_MAKE_PROTO(ep->gid, LCI_MSG_RDMA_SHORT, tag));
}

LCI_error_t LCI_putm(LCI_endpoint_t ep, LCI_mbuffer_t mbuffer, int rank,
                     LCI_tag_t tag, LCI_lbuffer_t remote_buffer,
                     uintptr_t remote_completion)
{
//  LC_POOL_GET_OR_RETN(ep->pkpool, p);
//  lc_pk_init(ep, (size > 1024) ? lc_pool_get_local(ep->pkpool) : -1, LC_PROTO_DATA, p);
//  struct lc_rep* rep = &(ep->rep[rank]);
//  memcpy(p->data.buffer, src, size);
//  lc_server_put(ep->server, rep->handle, rep->base, offset, rep->rkey, size, LCII_MAKE_PROTO(ep->gid, LC_PROTO_LONG, meta), p);
  return LCI_ERR_FEATURE_NA;
}

LCI_error_t LCI_putma(LCI_endpoint_t ep, LCI_mbuffer_t buffer, int rank,
                      LCI_tag_t tag, uintptr_t remote_completion) {
  LCM_DBG_Assert(remote_completion == LCI_UR_CQ_REMOTE, "Only support default remote completion (dynamic put to remote LCI_UR_CQ)\n");
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
                                   ep->device->heap.segment->mr_p,
                                   LCII_MAKE_PROTO(ep->gid, LCI_MSG_RDMA_MEDIUM, tag), ctx);
  if (ret == LCI_ERR_RETRY) {
    LCII_free_packet(packet);
    LCIU_free(ctx);
  }
  return ret;
}

LCI_error_t LCI_putmna(LCI_endpoint_t ep, LCI_mbuffer_t buffer, int rank,
                       LCI_tag_t tag, uintptr_t remote_completion) {
  LCM_DBG_Assert(remote_completion == LCI_UR_CQ_REMOTE, "Only support default remote completion (dynamic put to remote LCI_UR_CQ)\n");
  lc_packet* packet = LCII_mbuffer2packet(buffer);
  packet->context.poolid = (buffer.length > LCI_PACKET_RETURN_THRESHOLD) ?
                           lc_pool_get_local(ep->pkpool) : -1;

  LCII_context_t *ctx = LCIU_malloc(sizeof(LCII_context_t));
  ctx->data.mbuffer.address = (void*) packet->data.address;
  ctx->comp_type = LCI_COMPLETION_FREE;

  LCI_error_t ret = lc_server_send(ep->device->server, rank, packet->data.address, buffer.length,
                                   ep->device->heap.segment->mr_p,
                                   LCII_MAKE_PROTO(ep->gid, LCI_MSG_RDMA_MEDIUM, tag), ctx);
  if (ret == LCI_ERR_RETRY) {
    LCIU_free(ctx);
  }
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
                      uintptr_t remote_completion, void* user_context) {
  LCM_DBG_Assert(remote_completion == LCI_UR_CQ_REMOTE, "Only support default remote completion (dynamic put to remote LCI_UR_CQ)\n");

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

  packet->data.rts.msg_type = LCI_MSG_RDMA_LONG;
  packet->data.rts.send_ctx = (uintptr_t) rdv_ctx;
  packet->data.rts.size = buffer.length;

  LCM_DBG_Log(LCM_LOG_DEBUG, "send rts: type %d sctx %p size %lu\n",
              packet->data.rts.msg_type, (void*) packet->data.rts.send_ctx,
              packet->data.rts.size);
  LCI_error_t ret = lc_server_send(ep->device->server, rank, packet->data.address,
                                   sizeof(struct packet_rts), ep->device->heap.segment->mr_p,
                                   LCII_MAKE_PROTO(ep->gid, LCI_MSG_RTS, tag), rts_ctx);
  if (ret == LCI_ERR_RETRY) {
    LCII_free_packet(packet);
    LCIU_free(rts_ctx);
    LCIU_free(rdv_ctx);
  }
  return ret;
}