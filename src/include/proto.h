#ifndef LC_PROTO_H
#define LC_PROTO_H

/**
 * LCI internal protocol type
 * used to pass information to remote side
 */
typedef uint32_t LCII_proto_t;

#include "server/server.h"

#define LCII_MAKE_PROTO(rgid, msg_type, tag) (msg_type | (rgid << 3) | (tag << 16))

#define PROTO_GET_TYPE(proto) (proto & 0b0111)
#define PROTO_GET_RGID(proto) ((proto >> 3) & 0b01111111111111)
#define PROTO_GET_TAG(proto) ((proto >> 16) & 0xffff)

static inline void lc_handle_rts(LCI_endpoint_t ep, lc_packet* p, LCII_context_t *long_ctx)
{

  LCM_DBG_Assert(long_ctx->data.lbuffer.length >= p->data.rts.size,
       "the message sent by sendl is larger than the buffer posted by recvl!");
  long_ctx->data.lbuffer.length = p->data.rts.size;

  LCII_context_t *rtr_ctx = LCIU_malloc(sizeof(LCII_context_t));
  rtr_ctx->data.mbuffer.address = &(p->data);
  rtr_ctx->comp_type = LCI_COMPLETION_FREE;

  p->context.poolid = -1;
  p->data.rtr.ctx_id = long_ctx->reg_key;
  p->data.rtr.tgt_base = (uintptr_t) long_ctx->data.lbuffer.segment->address;
  p->data.rtr.tgt_offset =
      (uintptr_t) long_ctx->data.lbuffer.address - p->data.rtr.tgt_base;
  p->data.rtr.rkey = lc_server_rma_key(long_ctx->data.lbuffer.segment->mr_p);

  struct lc_rep* rep = &(ep->rep[long_ctx->rank]);
  lc_server_send(ep->server, rep->handle, p->data.address,
                 sizeof(struct packet_rtr), ep->server->heap_mr,
                 LCII_MAKE_PROTO(ep->gid, LCI_MSG_RTR, 0), rtr_ctx);
}

static inline void lc_handle_rtr(LCI_endpoint_t ep, lc_packet* packet)
{
  LCII_context_t *ctx = (LCII_context_t*) packet->data.rtr.ctx;

  lc_server_put(ep->server, ep->rep[ctx->rank].handle,
                ctx->data.lbuffer.address, ctx->data.lbuffer.length,
                ctx->data.lbuffer.segment->mr_p,
                packet->data.rtr.tgt_base, packet->data.rtr.tgt_offset,
                packet->data.rtr.rkey,
                LCII_MAKE_PROTO(ep->gid, LCI_MSG_LONG, packet->data.rtr.ctx_id),
                ctx);
  LCII_free_packet(packet);
}

static inline void lc_ce_dispatch(LCII_context_t *ctx)
{
  switch (ctx->comp_type) {
    case LCI_COMPLETION_NONE:
      LCIU_free(ctx);
      break;
    case LCI_COMPLETION_FREE:
      LCII_free_packet(LCII_mbuffer2packet(ctx->data.mbuffer));
      LCIU_free(ctx);
      break;
#ifdef LCI_SERVER_HAS_SYNC
    case LCI_COMPLETION_SYNC: {
      LCI_sync_signal(ctx->completion, ctx);
      break;
    }
#endif
#ifdef LCI_SERVER_HAS_CQ
    case LCI_COMPLETION_QUEUE:
      lc_cq_push(ctx->completion, ctx);
      break;
#endif
#ifdef LCI_SERVER_HAS_AM
    case LCI_COMPLETION_HANDLER: {
      LCI_handler_t* handler = ctx->completion;
      handler(LCII_ctx2req(ctx));
      LCIU_free(ctx);
      break;
    }
#endif
    default:
      LCM_DBG_Assert(false, "Unknown completion type: %d!\n", ctx->comp_type);
  }
}

static inline void lc_serve_recv(lc_packet* packet, uint32_t src_rank,
                                 size_t length, LCII_proto_t proto)
{
  // NOTE: this should be RGID because it is received from remote.
  LCI_endpoint_t ep = LCI_ENDPOINTS[PROTO_GET_RGID(proto)];
  uint16_t tag = PROTO_GET_TAG(proto);
  LCI_msg_type_t msg_type = PROTO_GET_TYPE(proto);

  switch (msg_type) {
    case LCI_MSG_SHORT:
    {
      LCM_DBG_Assert(length == LCI_SHORT_SIZE, "");
      lc_key key = LCII_MAKE_KEY(src_rank, ep->gid, tag, LCI_MSG_SHORT);
      lc_value value = (lc_value)packet;
      if (!lc_hash_insert(ep->mt, key, &value, SERVER)) {
        LCII_context_t* ctx = (LCII_context_t*)value;
        memcpy(&(ctx->data.immediate), packet->data.address, LCI_SHORT_SIZE);
        LCII_free_packet(packet);
        lc_ce_dispatch(ctx);
      }
      break;
    }
    case LCI_MSG_MEDIUM:
    {
      lc_key key = LCII_MAKE_KEY(src_rank, ep->gid, tag, LCI_MSG_MEDIUM);
      lc_value value = (lc_value)packet;
      packet->context.length = length;
      if (!lc_hash_insert(ep->mt, key, &value, SERVER)) {
        LCII_context_t* ctx = (LCII_context_t*)value;
        ctx->data.mbuffer.length = length;
        if (ctx->data.mbuffer.address != NULL) {
          // copy to user provided buffer
          memcpy(ctx->data.mbuffer.address, packet->data.address, ctx->data.mbuffer.length);
          LCII_free_packet(packet);
        } else {
          // use LCI packet
          ctx->data.mbuffer.address = packet->data.address;
        }
        lc_ce_dispatch(ctx);
      }
      break;
    }
    case LCI_MSG_RTS:
    {
      const lc_key key = LCII_MAKE_KEY(src_rank, ep->gid, tag, LCI_MSG_LONG);
      lc_value value = (lc_value)packet;
      if (!lc_hash_insert(ep->mt, key, &value, SERVER)) {
        lc_handle_rts(ep, packet, (LCI_comp_t)value);
      }
      break;
    }
    case LCI_MSG_RTR:
      lc_handle_rtr(ep, packet);
      break;
    case LCI_MSG_RDMA_SHORT:
    {
      // dynamic put
      LCM_DBG_Assert(length == LCI_SHORT_SIZE, "");
      LCII_context_t* ctx = LCIU_malloc(sizeof(LCII_context_t));
      memcpy(&(ctx->data.immediate), packet->data.address, LCI_SHORT_SIZE);
      LCII_free_packet(packet);
      ctx->data_type = LCI_IMMEDIATE;
      ctx->rank = src_rank;
      ctx->tag = tag;
      ctx->comp_type = ep->msg_comp_type;
      ctx->completion = LCI_UR_CQ;
      ctx->user_context = NULL;
      lc_ce_dispatch(ctx);
      break;
    }
    case LCI_MSG_RDMA_MEDIUM:
    {
      LCII_context_t* ctx = LCIU_malloc(sizeof(LCII_context_t));
      ctx->data.mbuffer.address = packet->data.address;
      ctx->data.mbuffer.length = length;
      ctx->data_type = LCI_MEDIUM;
      ctx->rank = src_rank;
      ctx->tag = tag;
      ctx->comp_type = ep->msg_comp_type;
      ctx->completion = LCI_UR_CQ;
      ctx->user_context = NULL;
      lc_ce_dispatch(ctx);
      break;
    }
    default:
      LCM_DBG_Assert(false, "unknown proto!");
  }
}

static inline void lc_serve_rdma(LCII_proto_t proto)
{
  LCI_endpoint_t ep = LCI_ENDPOINTS[PROTO_GET_RGID(proto)];
  uint16_t tag = PROTO_GET_TAG(proto);
  LCI_msg_type_t msg_type = PROTO_GET_TYPE(proto);

  switch (msg_type) {
    case LCI_MSG_LONG: {
      LCII_context_t *ctx =
          (LCII_context_t*)LCII_register_remove(ep->ctx_reg, tag);
      LCM_DBG_Assert(ctx->data_type == LCI_LONG,
                     "Didn't get the right context! This might imply some bugs in the lcii_register.");
      // recvl has been completed locally. Need to process completion.
      lc_ce_dispatch(ctx);
      break;
    }
    default:
      LCM_DBG_Assert(false, "unknown proto!");
  }
}

// local completion
static inline void lc_serve_send(void* raw_ctx)
{
  LCII_context_t *ctx = raw_ctx;
  lc_ce_dispatch(ctx);
}

#endif
