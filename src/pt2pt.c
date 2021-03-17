#include "lci.h"
#include "lcii.h"

LCI_error_t LCI_sends(LCI_endpoint_t ep, LCI_short_t src, int rank, LCI_tag_t tag)
{
  struct lc_rep* rep = &(ep->rep[rank]);
  lc_server_sends(ep->server, rep->handle, &src, sizeof(LCI_short_t),
                  LCII_MAKE_PROTO(ep->gid, LCI_MSG_SHORT, tag));
  return LCI_OK;
}

LCI_error_t LCI_sendm(LCI_endpoint_t ep, LCI_mbuffer_t buffer, int rank,
                      LCI_tag_t tag)
{
  lc_packet* packet = lc_pool_get_nb(ep->pkpool);
  if (packet == NULL)
    // no packet is available
    return LCI_ERR_RETRY;
  packet->context.poolid = (buffer.length > LCI_PACKET_RETURN_THRESHOLD) ?
                          lc_pool_get_local(ep->pkpool) : -1;
  memcpy(packet->data.address, buffer.address, buffer.length);

  LCII_context_t *ctx = LCIU_malloc(sizeof(struct LCII_context_t));
  ctx->data.mbuffer.address = (void*) packet->data.address;
  ctx->msg_type = LCI_MSG_MEDIUM;

  struct lc_rep* rep = &(ep->rep[rank]);
  lc_server_send(ep->server, rep->handle, packet->data.address, buffer.length,
                  ep->server->heap_mr,
                 LCII_MAKE_PROTO(ep->gid, LCI_MSG_MEDIUM, tag), ctx);
  return LCI_OK;
}

LCI_error_t LCI_sendmn(LCI_endpoint_t ep, LCI_mbuffer_t buffer, int rank,
                       LCI_tag_t tag)
{
  lc_packet* packet = LCII_mbuffer2packet(buffer);
  packet->context.poolid = (buffer.length > LCI_PACKET_RETURN_THRESHOLD) ?
                           lc_pool_get_local(ep->pkpool) : -1;

  LCII_context_t *ctx = LCIU_malloc(sizeof(struct LCII_context_t));
  ctx->data.mbuffer.address = (void*) packet->data.address;
  ctx->msg_type = LCI_MSG_MEDIUM;

  struct lc_rep* rep = &(ep->rep[rank]);
  lc_server_send(ep->server, rep->handle, packet->data.address, buffer.length,
                 ep->server->heap_mr,
                 LCII_MAKE_PROTO(ep->gid, LCI_MSG_MEDIUM, tag), ctx);
  return LCI_OK;
}

LCI_error_t LCI_sendl(LCI_endpoint_t ep, LCI_lbuffer_t buffer, uint32_t rank,
                      LCI_tag_t tag, LCI_comp_t completion, void* user_context)
{
  lc_packet* p = lc_pool_get_nb(ep->pkpool);
  if (p == NULL)
    // no packet is available
    return LCI_ERR_RETRY;
  p->context.poolid = -1;

  LCII_context_t *rts_ctx = LCIU_malloc(sizeof(struct LCII_context_t));
  rts_ctx->data.mbuffer.address = (void*) &(p->data);
  rts_ctx->msg_type = LCI_MSG_RTS;

  LCII_context_t *long_ctx = LCIU_malloc(sizeof(struct LCII_context_t));
  long_ctx->ep = ep;
  long_ctx->data.lbuffer = buffer;
  long_ctx->data_type = LCI_LONG;
  long_ctx->msg_type = LCI_MSG_LONG;
  long_ctx->rank = rank;
  long_ctx->tag = tag;
  long_ctx->user_context = user_context;
  long_ctx->completion = completion;

  p->data.rts.ctx = (uintptr_t) long_ctx;
  p->data.rts.size = buffer.length;

  struct lc_rep* rep = &(ep->rep[rank]);
  lc_server_send(ep->server, rep->handle, p->data.address,
                  sizeof(struct packet_rts), ep->server->heap_mr,
                  LCII_MAKE_PROTO(ep->gid, LCI_MSG_RTS, tag), rts_ctx);
  return LCI_OK;
}

LCI_error_t LCI_recvs(LCI_endpoint_t ep, int rank, LCI_tag_t tag,
                      LCI_comp_t completion, void* user_context)
{
  LCII_context_t *ctx = LCIU_malloc(sizeof(LCII_context_t));
  ctx->ep = ep;
  ctx->data_type = LCI_IMMEDIATE;
  ctx->msg_type = LCI_MSG_SHORT;
  ctx->rank = rank;
  ctx->tag = tag;
  ctx->user_context = user_context;
  ctx->completion = completion;

  lc_key key = LCII_MAKE_KEY(rank, ep->gid, tag, LCI_MSG_SHORT);
  lc_value value = (lc_value)ctx;
  if (!lc_hash_insert(ep->mt, key, &value, CLIENT)) {
    lc_packet* packet = (lc_packet*) value;
    memcpy(&(ctx->data.immediate), packet->data.address, LCI_SHORT_SIZE);
    LCII_free_packet(packet);
    lc_ce_dispatch(ep->msg_comp_type, ctx);
  }
  return LCI_OK;
}

LCI_error_t LCI_recvm(LCI_endpoint_t ep, LCI_mbuffer_t buffer, int rank,
                      LCI_tag_t tag, LCI_comp_t completion, void* user_context)
{
  LCII_context_t *ctx = LCIU_malloc(sizeof(LCII_context_t));
  ctx->ep = ep;
  ctx->data.mbuffer = buffer;
  ctx->data_type = LCI_MEDIUM;
  ctx->msg_type = LCI_MSG_MEDIUM;
  ctx->rank = rank;
  ctx->tag = tag;
  ctx->user_context = user_context;
  ctx->completion = completion;

  lc_key key = LCII_MAKE_KEY(rank, ep->gid, tag, LCI_MSG_MEDIUM);
  lc_value value = (lc_value)completion;
  if (!lc_hash_insert(ep->mt, key, &value, CLIENT)) {
    lc_packet* packet = (lc_packet*) value;
    ctx->data.mbuffer.length = packet->context.length;
    // copy to user provided buffer
    memcpy(ctx->data.mbuffer.address, packet->data.address, ctx->data.mbuffer.length);
    LCII_free_packet(packet);
    lc_ce_dispatch(ep->msg_comp_type, ctx);
  }
  return LCI_OK;
}

LCI_error_t LCI_recvmn(LCI_endpoint_t ep, int rank, LCI_tag_t tag,
                       LCI_comp_t completion, void* user_context)
{
  LCII_context_t *ctx = LCIU_malloc(sizeof(LCII_context_t));
  ctx->ep = ep;
  ctx->data.mbuffer.address = NULL;
  ctx->data_type = LCI_MEDIUM;
  ctx->msg_type = LCI_MSG_MEDIUM;
  ctx->rank = rank;
  ctx->tag = tag;
  ctx->user_context = user_context;
  ctx->completion = completion;

  lc_key key = LCII_MAKE_KEY(rank, ep->gid, tag, LCI_MSG_MEDIUM);
  lc_value value = (lc_value)completion;
  if (!lc_hash_insert(ep->mt, key, &value, CLIENT)) {
    lc_packet* packet = (lc_packet*) value;
    ctx->data.mbuffer.length = packet->context.length;
    // use LCI packet
    ctx->data.mbuffer.address = packet->data.address;
    lc_ce_dispatch(ep->msg_comp_type, ctx);
  }
  return LCI_OK;
}

LCI_error_t LCI_recvl(LCI_endpoint_t ep, LCI_lbuffer_t buffer, uint32_t rank,
                      LCI_tag_t tag, LCI_comp_t completion, void* user_context)
{
  LCII_context_t *long_ctx = LCIU_malloc(sizeof(LCII_context_t));
  LCI_error_t ret = LCII_register_put(ep->ctx_reg, (LCII_reg_value_t)long_ctx,
                                      (LCII_reg_key_t*)&(long_ctx->id));
  if (ret != LCI_OK) {
    LCIU_free(long_ctx);
    return ret;
  }
  long_ctx->ep = ep;
  long_ctx->data.lbuffer = buffer;
  long_ctx->data_type = LCI_LONG;
  long_ctx->msg_type = LCI_MSG_LONG;
  long_ctx->rank = rank;
  long_ctx->tag = tag;
  long_ctx->user_context = user_context;
  long_ctx->completion = completion;

  lc_key key = LCII_MAKE_KEY(rank, ep->gid, tag, LCI_MSG_LONG);
  lc_value value = (lc_value)long_ctx;
  if (!lc_hash_insert(ep->mt, key, &value, CLIENT)) {
    lc_packet* p = (lc_packet*) value;
    lc_handle_rts(ep, p, long_ctx);
  }
  return LCI_OK;
}
