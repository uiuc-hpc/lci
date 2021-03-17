#ifndef LC_PROTO_H
#define LC_PROTO_H

#include "config.h"

#include "lci.h"
#include "log.h"
#include "cq.h"
#include "hashtable.h"
#include "lciu.h"

/**
 * LCI internal protocol type
 * used to pass information to remote side
 */
typedef uint32_t LCII_proto_t;

#include "server/server.h"

#define LCII_MAKE_PROTO(rgid, msg_type, tag) (msg_type | (rgid << 3) | (tag << 16))

#define PROTO_GET_TYPE(proto) (proto & 0b011)
#define PROTO_GET_RGID(proto) ((proto >> 3) & 0b01111111111111)
#define PROTO_GET_TAG(proto) ((proto >> 16) & 0xffff)

static inline void lc_handle_rts(LCI_endpoint_t ep, lc_packet* p, LCII_context_t *long_ctx)
{

  LCI_DBG_Assert(long_ctx->data.lbuffer.length >= p->data.rts.size,
       "the message sent by sendl is larger than the buffer posted by recvl!");
  long_ctx->data.lbuffer.length = p->data.rts.size;

  LCII_context_t *rtr_ctx = LCIU_malloc(sizeof(LCII_context_t));
  rtr_ctx->data.mbuffer.address = &(p->data);
  rtr_ctx->msg_type = LCI_MSG_RTR;

  p->context.poolid = -1;
  p->data.rtr.ctx_id = long_ctx->id;
  p->data.rtr.tgt_addr = (uintptr_t) long_ctx->data.lbuffer.address;
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
                ctx->data.lbuffer.segment->mr_p, 0,
                packet->data.rtr.tgt_addr, packet->data.rtr.rkey,
                LCII_MAKE_PROTO(ep->gid, LCI_MSG_LONG, packet->data.rtr.ctx_id),
                ctx);
}

static inline void lc_ce_dispatch(LCI_comptype_t comp_type, LCII_context_t *ctx)
{
  switch (comp_type) {
#ifdef LCI_SERVER_HAS_SYNC
    case LCI_COMPLETION_ONE2ONEL: {
      LCI_syncl_t* sync = ctx->completion;
      sync->request = LCII_ctx2req(ctx);
      LCI_one2one_set_full(sync);
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
      break;
    }
#endif
    default:
      LCI_DBG_Assert(false, "unknown proto!");
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
      LCI_DBG_Assert(length == LCI_SHORT_SIZE, "");
      lc_key key = LCII_MAKE_KEY(src_rank, ep->gid, tag, LCI_MSG_SHORT);
      lc_value value = (lc_value)packet;
      if (!lc_hash_insert(ep->mt, key, &value, SERVER)) {
        LCII_context_t* ctx = (LCII_context_t*)value;
        memcpy(&(ctx->data.immediate), packet->data.address, LCI_SHORT_SIZE);
        LCII_free_packet(packet);
        lc_ce_dispatch(ep->msg_comp_type, ctx);
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
        lc_ce_dispatch(ep->msg_comp_type, ctx);
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
    default:
      LCI_DBG_Assert(false, "unknown proto!");
  }
}

static inline void lc_serve_rdma(LCII_proto_t proto)
{
  LCI_endpoint_t ep = LCI_ENDPOINTS[PROTO_GET_RGID(proto)];
  uint16_t tag = PROTO_GET_TAG(proto);
  LCI_msg_type_t msg_type = PROTO_GET_TYPE(proto);

  switch (msg_type) {
    case LCI_MSG_LONG:
    {
      LCII_context_t *ctx =
          (LCII_context_t*)LCII_register_get(ep->ctx_reg, tag);
      LCI_DBG_Assert(ctx->msg_type == LCI_MSG_LONG,
                     "Didn't get the right context! This might imply some bugs in the lcii_register.");
      // recvl has been completed locally. Need to process completion.
      lc_ce_dispatch(ep->msg_comp_type, ctx);
      break;
    }
    case LCI_MSG_RDMA:
    {
      break;
    }
    default:
      LCI_DBG_Assert(false, "unknown proto!");
  }
}

// local completion
static inline void lc_serve_send(void* raw_ctx)
{
  LCII_context_t *ctx = raw_ctx;
  LCI_endpoint_t ep = ctx->ep;
//  LCI_msg_type_t proto = p->context.proto;
  switch (ctx->msg_type) {
    case LCI_MSG_MEDIUM:
      LCII_free_packet(LCII_mbuffer2packet(ctx->data.mbuffer));
      break;
    case LCI_MSG_LONG:
      // sendl has been completed locally. Need to process completion.
      lc_ce_dispatch(ep->cmd_comp_type, ctx);
      break;
    case LCI_MSG_RTS:
      // sendl has not been completed locally. No need to process completion.
      LCII_free_packet(LCII_mbuffer2packet(ctx->data.mbuffer));
      break;
    case LCI_MSG_RTR:
      // recvl has not been completed locally. No need to process completion.
      LCII_free_packet(LCII_mbuffer2packet(ctx->data.mbuffer));
      break;
    default:
      LCI_DBG_Assert(false, "unexpected message type %d\n", ctx->msg_type);
  }
}

#endif
