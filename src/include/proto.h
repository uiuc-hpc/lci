#ifndef LC_PROTO_H
#define LC_PROTO_H

// 32 bits for rank, 2 bits for msg type, 14 bits for endpoint ID, 16 bits for tag
static inline uint64_t LCII_make_key(LCI_endpoint_t ep, int rank, LCI_tag_t tag,
                                     LCI_msg_type_t msg_type) {
  uint64_t ret = 0;
  if (ep->match_type == LCI_MATCH_RANKTAG) {
    ret = (uint64_t)(rank) << 32 | (uint64_t) (msg_type) << 30 |
          (uint64_t)(ep->gid) << 16 | (uint64_t)(tag);
  } else {
    LCM_DBG_Assert(ep->match_type == LCI_MATCH_TAG, "Unknown match_type %d\n", ep->match_type);
    ret = (uint64_t)(-1) << 32 | (uint64_t) (msg_type) << 30 |
          (uint64_t)(ep->gid) << 16 | (uint64_t)(tag);
  }
  return ret;
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
      LCII_queue_push(ctx->completion, ctx);
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

static inline void lc_serve_recv(lc_packet* packet,
                                 int src_rank, size_t length,
                                 LCII_proto_t proto)
{
  // NOTE: this should be RGID because it is received from remote.
  LCI_endpoint_t ep = LCI_ENDPOINTS[PROTO_GET_RGID(proto)];
  LCI_tag_t tag = PROTO_GET_TAG(proto);
  LCI_msg_type_t msg_type = PROTO_GET_TYPE(proto);
  packet->context.src_rank = src_rank;

  switch (msg_type) {
    case LCI_MSG_SHORT:
    {
      LCM_DBG_Assert(length == LCI_SHORT_SIZE, "Unexpected message length %lu\n", length);
      lc_key key = LCII_make_key(ep, src_rank, tag, LCI_MSG_SHORT);
      lc_value value = (lc_value)packet;
      if (!lc_hash_insert(ep->mt, key, &value, SERVER)) {
        LCII_context_t* ctx = (LCII_context_t*)value;
        // If the receiver uses LCI_MATCH_TAG, we have to set the rank here.
        ctx->rank = src_rank;
        memcpy(&(ctx->data.immediate), packet->data.address, LCI_SHORT_SIZE);
        LCII_free_packet(packet);
        lc_ce_dispatch(ctx);
      }
      break;
    }
    case LCI_MSG_MEDIUM:
    {
      lc_key key = LCII_make_key(ep, src_rank, tag, LCI_MSG_MEDIUM);
      lc_value value = (lc_value)packet;
      packet->context.length = length;
      if (!lc_hash_insert(ep->mt, key, &value, SERVER)) {
        LCII_context_t* ctx = (LCII_context_t*)value;
        ctx->rank = src_rank;
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
      switch (packet->data.rts.msg_type) {
        case LCI_MSG_LONG:
        {
          const lc_key key =
              LCII_make_key(ep, src_rank, tag, LCI_MSG_LONG);
          lc_value value = (lc_value)packet;
          if (!lc_hash_insert(ep->mt, key, &value, SERVER)) {
            LCII_handle_2sided_rts(ep, packet, (LCI_comp_t)value);
          }
          break;
        }
        case LCI_MSG_RDMA_LONG:
        {
          LCII_handle_1sided_rts(ep, packet, src_rank, tag);
          break;
        }
        default:
          LCM_Assert(false, "Unknown message type %d!\n", packet->data.rts.msg_type);
      }
      break;
    }
    case LCI_MSG_RTR:
      switch (packet->data.rts.msg_type) {
        case LCI_MSG_LONG: {
          LCII_handle_2sided_rtr(ep, packet);
          break;
        }
        case LCI_MSG_RDMA_LONG:
        {
          LCII_handle_1sided_rtr(ep, packet);
          break;
        }
        default:
          LCM_Assert(false, "Unknown message type %d!\n", packet->data.rts.msg_type);
      }
      break;
    case LCI_MSG_RDMA_SHORT:
    {
      // dynamic put
      LCM_DBG_Assert(length == LCI_SHORT_SIZE, "Unexpected message length %lu\n", length);
      LCII_context_t* ctx = LCIU_malloc(sizeof(LCII_context_t));
      memcpy(&(ctx->data.immediate), packet->data.address, LCI_SHORT_SIZE);
      LCII_free_packet(packet);
      ctx->data_type = LCI_IMMEDIATE;
      ctx->rank = src_rank;
      ctx->tag = tag;
      ctx->comp_type = LCI_COMPLETION_QUEUE;
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
      ctx->comp_type = LCI_COMPLETION_QUEUE;
      ctx->completion = LCI_UR_CQ;
      ctx->user_context = NULL;
      lc_ce_dispatch(ctx);
      break;
    }
    default:
      LCM_Assert(false, "Unknown proto!");
  }
}

static inline void lc_serve_rdma(LCII_proto_t proto)
{
  LCI_endpoint_t ep = LCI_ENDPOINTS[PROTO_GET_RGID(proto)];
  uint16_t tag = PROTO_GET_TAG(proto);
  LCI_msg_type_t msg_type = PROTO_GET_TYPE(proto);

  switch (msg_type) {
    case LCI_MSG_LONG: {
      LCII_handle_2sided_writeImm(ep, tag);
      break;
    }
    case LCI_MSG_RDMA_LONG: {
      LCII_handle_1sided_writeImm(ep, tag);
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
