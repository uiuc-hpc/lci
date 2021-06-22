#ifndef LCI_LCII_RDV_H
#define LCI_LCII_RDV_H

#define LCII_RDV_RETRY(stat, n, msg) \
  LCI_error_t ret;                   \
  do {                               \
      ret = stat;                    \
      if (ret == LCI_OK) break;      \
  } while (ret == LCI_ERR_RETRY);    \
  LCM_Assert(ret == LCI_OK, msg);

static inline void LCII_handle_2sided_rts(LCI_endpoint_t ep, lc_packet* packet, LCII_context_t *rdv_ctx)
{

  LCM_DBG_Assert(rdv_ctx->data.lbuffer.length >= packet->data.rts.size,
                 "the message sent by sendl is larger than the buffer posted by recvl!");
  rdv_ctx->data.lbuffer.length = packet->data.rts.size;

  LCII_context_t *rtr_ctx = LCIU_malloc(sizeof(LCII_context_t));
  rtr_ctx->data.mbuffer.address = &(packet->data);
  rtr_ctx->comp_type = LCI_COMPLETION_FREE;

  packet->context.poolid = -1;
  uint64_t ctx_key;
  int result = LCM_archive_put(ep->ctx_archive, (uintptr_t)rdv_ctx, &ctx_key);
  // TODO: be able to pass back pressure to user
  LCM_Assert(result == LCM_SUCCESS, "Archive is full!\n");
  packet->data.rtr.recv_ctx_key = ctx_key;
  packet->data.rtr.remote_addr_base = (uintptr_t) rdv_ctx->data.lbuffer.segment->address;
  packet->data.rtr.remote_addr_offset =
      (uintptr_t) rdv_ctx->data.lbuffer.address - packet->data.rtr.remote_addr_base;
  packet->data.rtr.rkey = lc_server_rma_rkey(rdv_ctx->data.lbuffer.segment->mr_p);

  LCII_RDV_RETRY(lc_server_send(ep->device->server, rdv_ctx->rank, packet->data.address,
                                    sizeof(struct packet_rtr), ep->device->heap.segment->mr_p,
                                    LCII_MAKE_PROTO(ep->gid, LCI_MSG_RTR, 0), rtr_ctx),
                 10, "Cannot send RTR message!\n")

}

static inline void LCII_handle_2sided_rtr(LCI_endpoint_t ep, lc_packet* packet)
{
  LCII_context_t *ctx = (LCII_context_t*) packet->data.rtr.send_ctx;

  LCII_RDV_RETRY(lc_server_put(ep->device->server, ctx->rank,
                                  ctx->data.lbuffer.address, ctx->data.lbuffer.length,
                                  ctx->data.lbuffer.segment->mr_p,
                                  packet->data.rtr.remote_addr_base, packet->data.rtr.remote_addr_offset,
                                  packet->data.rtr.rkey,
                                  LCII_MAKE_PROTO(ep->gid, LCI_MSG_LONG, packet->data.rtr.recv_ctx_key),
                                  ctx), 10, "Cannot post RDMA writeImm\n");
  LCII_free_packet(packet);
}

static inline void LCII_handle_2sided_writeImm(LCI_endpoint_t ep, uint64_t ctx_key)
{
  LCII_context_t *ctx =
      (LCII_context_t*)LCM_archive_remove(ep->ctx_archive, ctx_key);
  LCM_DBG_Assert(ctx->data_type == LCI_LONG,
                 "Didn't get the right context! This might imply some bugs in the LCM_archive_t.");
  // recvl has been completed locally. Need to process completion.
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
  rdv_ctx->comp_type = LCI_COMPLETION_QUEUE;
  rdv_ctx->completion = LCI_UR_CQ;

  LCII_context_t *rtr_ctx = LCIU_malloc(sizeof(LCII_context_t));
  rtr_ctx->data.mbuffer.address = &(packet->data);
  rtr_ctx->comp_type = LCI_COMPLETION_FREE;

  // reuse the rts packet as rtr packet
  packet->context.poolid = -1;
  uint64_t ctx_key;
  int result = LCM_archive_put(ep->ctx_archive, (uintptr_t)rdv_ctx, &ctx_key);
  // TODO: be able to pass back pressure to user
  LCM_Assert(result == LCM_SUCCESS, "Archive is full!\n");
  packet->data.rtr.recv_ctx_key = ctx_key;
  packet->data.rtr.remote_addr_base = (uintptr_t) rdv_ctx->data.lbuffer.segment->address;
  packet->data.rtr.remote_addr_offset =
      (uintptr_t) rdv_ctx->data.lbuffer.address - packet->data.rtr.remote_addr_base;
  packet->data.rtr.rkey = lc_server_rma_rkey(rdv_ctx->data.lbuffer.segment->mr_p);

  LCII_RDV_RETRY(lc_server_send(ep->device->server, rdv_ctx->rank, packet->data.address,
                                   sizeof(struct packet_rtr), ep->device->heap.segment->mr_p,
                                   LCII_MAKE_PROTO(ep->gid, LCI_MSG_RTR, 0), rtr_ctx),
                 10, "Cannot send RTR message!\n");
}

static inline void LCII_handle_1sided_rtr(LCI_endpoint_t ep, lc_packet* packet)
{
  LCII_context_t *ctx = (LCII_context_t*) packet->data.rtr.send_ctx;

  LCII_RDV_RETRY(lc_server_put(ep->device->server, ctx->rank,
                                  ctx->data.lbuffer.address, ctx->data.lbuffer.length,
                                  ctx->data.lbuffer.segment->mr_p,
                                  packet->data.rtr.remote_addr_base, packet->data.rtr.remote_addr_offset,
                                  packet->data.rtr.rkey,
                                  LCII_MAKE_PROTO(ep->gid, LCI_MSG_RDMA_LONG, packet->data.rtr.recv_ctx_key),
                                  ctx),
                 10, "Cannot post RDMA writeImm!\n");
  LCII_free_packet(packet);
}

static inline void LCII_handle_1sided_writeImm(LCI_endpoint_t ep, uint64_t ctx_key)
{
  LCII_context_t *ctx =
      (LCII_context_t*)LCM_archive_remove(ep->ctx_archive, ctx_key);
  LCM_DBG_Assert(ctx->data_type == LCI_LONG,
                 "Didn't get the right context! This might imply some bugs in the LCM_archive_t.");
  // recvl has been completed locally. Need to process completion.
  lc_ce_dispatch(ctx);
}

#endif  // LCI_LCII_RDV_H
