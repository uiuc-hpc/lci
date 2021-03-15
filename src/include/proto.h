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

#define MAKE_PROTO(rgid, msg_type, tag) (msg_type | (rgid << 3) | (tag << 16))

#define PROTO_GET_PROTO(proto) (proto & 0b011)
#define PROTO_GET_RGID(proto) ((proto >> 3) & 0b01111111111111)
#define PROTO_GET_TAG(proto) ((proto >> 16) & 0xffff)

/* CRC-32C (iSCSI) polynomial in reversed bit order. */
#define POLY 0x82f63b78
inline uint32_t crc32c(char* buf, size_t len)
{
  uint32_t crc = 0;
  int k;

  crc = ~crc;
  while (len--) {
    crc ^= *buf++;
    for (k = 0; k < 8; k++) crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
  }
  return ~crc;
}

static inline void lc_init_req(void* buf, size_t size, LCI_request_t* req)
{
  req->data.buffer.start = buf;
  req->data.buffer.length = size;
  req->type = DIRECT;
}

static inline void lc_prepare_rts(void* src, size_t size, void* usr_context,
                                  lc_packet* p)
{
  p->data.rts.src_addr = (uintptr_t)src;
  p->data.rts.size = size;
  p->data.rts.ctx = (uintptr_t)usr_context;
  LCII_MEM_FENCE();
}

static inline void lc_handle_rtr(LCI_endpoint_t ep, lc_packet* p)
{
  lc_pk_init(ep, -1, LC_PROTO_LONG, p);
  int tgt_rank = p->context.sync->request.rank;

  p->context.sync = (LCI_syncl_t*) p->data.rts.ctx;
  lc_server_rma_rtr(ep->server, ep->server->rep[tgt_rank].handle,
                    (void*)p->data.rts.src_addr, p->data.rtr.tgt_addr,
                    p->data.rtr.rkey, p->data.rts.size, p->data.rtr.comm_id, p);
}

static inline void lc_handle_rts(LCI_endpoint_t ep, lc_packet* p, LCII_context_t *long_ctx)
{

  LCI_DBG_Assert(long_ctx->data.lbuffer.length >= p->data.rts.size,
       "the message sent by sendl is larger than the buffer posted by recvl!");
  long_ctx->data.lbuffer.length = p->data.rts.size;

  LCII_context_t *rtr_ctx = LCIU_malloc(sizeof(LCII_context_t));
  rtr_ctx->data.mbuffer.address = &(p->data);
  rtr_ctx->msg_type = LCI_MSG_RTR;

  p->context.poolid = -1;
  p->data.rtr.ctx_id = (uint32_t) &long_ctx; // TODO
  p->data.rtr.tgt_addr = (uintptr_t) long_ctx->data.lbuffer.address;
  p->data.rtr.rkey = lc_server_rma_key(long_ctx->data.lbuffer.segment->mr_p);

  struct lc_rep* rep = &(ep->rep[long_ctx->rank]);
  lc_server_send(ep->server, rep->handle, p->data.address, sizeof(struct packet_rtr),
                  rtr_ctx, MAKE_PROTO(ep->gid, LCI_MSG_RTR, 0));
}

static inline void lc_ce_dispatch(LCI_endpoint_t ep, LCII_context_t *ctx);

static inline void lc_serve_recv_imm(LCI_endpoint_t ep, lc_packet* p,
                                     uint16_t tag, const long cap)
{
  p->context.sync->request.tag = tag;
  p->context.sync->request.data.buffer.start = &p->data;
  lc_ce_dispatch(ep, p, p->context.sync, cap);
}

static inline void lc_serve_recv_dyn(LCI_endpoint_t ep, lc_packet* p,
                                     uint32_t proto, uint16_t tag, const long cap)
{
  p->context.sync->request.tag = tag;

  if (proto == LC_PROTO_DATA) {
    void* buf = ep->alloc.malloc(p->context.sync->request.data.buffer.length, 0);
    memcpy(buf, &p->data, p->context.sync->request.data.buffer.length);
    p->context.sync->request.data.buffer.start = buf;
    lc_ce_dispatch(ep, p, p->context.sync, cap);
  } else if (proto == LCI_MSG_RTS) {
    void* buf = ep->alloc.malloc(p->data.rts.size, 0);
    lc_init_req(buf, p->data.rts.size, &p->context.sync->request);
    lc_handle_rts(ep, p);
  } else if (proto == LCI_MSG_RTR) {
    lc_handle_rtr(ep, p);
  };
}

static inline void lc_serve_recv_match(LCI_endpoint_t ep, lc_packet* p,
                                      uint32_t proto, uint16_t tag, const long cap)
{
  p->context.sync->request.tag = tag;
  p->context.proto = proto;

  if (proto == LC_PROTO_DATA) {
    const lc_key key = LCII_MAKE_KEY(p->context.sync->request.rank,
                                   p->context.sync->request.tag);
    lc_value value = (lc_value)p;
    if (!lc_hash_insert(ep->mt, key, &value, SERVER)) {
      LCI_syncl_t* sync = (LCI_syncl_t*)value;
      sync->request.data.buffer.length = p->context.sync->request.data.buffer.length;
      memcpy(sync->request.data.buffer.start, p->data.buffer,
             p->context.sync->request.data.buffer.length);
      lc_ce_dispatch(ep, p, sync, cap);
    }
  } else if (proto == LCI_MSG_RTS) {
    const lc_key key = LCII_MAKE_KEY(p->context.sync->request.rank,
                                   p->context.sync->request.tag);
    lc_value value = (lc_value)p;
    if (!lc_hash_insert(ep->mt, key, &value, SERVER)) {
      p->context.sync = (LCI_syncl_t*)value;
      lc_handle_rts(ep, p);
    }
  } else if (proto == LCI_MSG_RTR) {
    lc_handle_rtr(ep, p);
  };
}

static inline void lc_serve_recv_dispatch(LCI_endpoint_t ep, lc_packet* p,
                                          uint32_t proto, uint16_t meta, const long cap)
{
  if (proto == LC_PROTO_LONG) {
    lc_serve_recv_imm(ep, p, meta, cap);
    return;
  }

#ifdef LCI_SERVER_HAS_EXP
  if (cap & EP_AR_EXP) {
    return lc_serve_recv_match(ep, p, proto, meta, cap);
  } else
#endif
#ifdef LCI_SERVER_HAS_DYN
  if (cap & EP_AR_DYN) {
    return lc_serve_recv_dyn(ep, p, proto, meta, cap);
  } else
#endif
#ifdef LCI_SERVER_HAS_IMM
  if (cap & EP_AR_IMM) {
    return lc_serve_recv_imm(ep, p, meta, cap);
  } else
#endif
  // placeholder for anything else.
  {
  }
}

static inline LCI_request_t LCII_ctx2req(LCII_context_t *ctx) {
  LCI_request_t request = {
    .flag = LCI_OK,
    .rank = ctx->rank,
    .tag = ctx->tag,
    .type = ctx->data_type,
    .data = ctx->data,
    .user_context = ctx->user_context
  };
  LCIU_free(ctx);
}

static inline void lc_ce_dispatch(LCI_endpoint_t ep, LCII_context_t *ctx)
{
  uint64_t cap = ep->property;
#ifdef LCI_SERVER_HAS_SYNC
  if (cap & EP_CE_SYNC) {
    LCI_one2one_set_full(ctx->comp);
    LCIU_free(ctx);
  } else
#endif
#ifdef LCI_SERVER_HAS_CQ
  if (cap & EP_CE_CQ) {
    lc_cq_push(ep->cq, ctx);
  } else
#endif
#ifdef LCI_SERVER_HAS_AM
  if (cap & EP_CE_AM) {
    ep->handler(LCII_ctx2req(ctx));
  } else
#endif
#ifdef LCI_SERVER_HAS_GLOB
  if (cap & EP_CE_GLOB) {
    __sync_fetch_and_add(&ep->completed, 1);
    LCIU_free(ctx);
  } else
#endif
  {
    LCI_DBG_Assert(false, "unknown proto!");
  }
}

static inline void lc_serve_recv(lc_packet* packet, uint32_t src_rank,
                                 size_t length, LCII_proto_t raw_proto)
{
  // NOTE: this should be RGID because it is received from remote.
  LCI_endpoint_t ep = LCI_ENDPOINTS[PROTO_GET_RGID(raw_proto)];
  uint16_t tag = PROTO_GET_TAG(raw_proto);
  LCI_msg_type_t msg_type = PROTO_GET_PROTO(raw_proto);

  switch (msg_type) {
    case LCI_MSG_SHORT:
    {
      LCI_DBG_Assert(length == LCI_SHORT_SIZE, "");
      lc_key key = LCII_MAKE_KEY(src_rank, ep->gid, tag);
      lc_value value = (lc_value)packet;
      if (!lc_hash_insert(ep->mt, key, &value, SERVER)) {
        LCII_context_t* ctx = (LCII_context_t*)value;
        memcpy(&(ctx->data.immediate), packet->data.address, LCI_SHORT_SIZE);
        LCII_free_packet(packet);
        lc_ce_dispatch(ep, ctx);
      }
      break;
    }
    case LCI_MSG_MEDIUM:
    {
      lc_key key = LCII_MAKE_KEY(src_rank, ep->gid, tag);
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
        lc_ce_dispatch(ep, ctx);
      }
      break;
    }
    case LCI_MSG_RTS:
      const lc_key key = LCII_MAKE_KEY(src_rank, ep->gid, tag);
      lc_value value = (lc_value) p;
      if (!lc_hash_insert(ep->mt, key, &value, SERVER)) {
        lc_handle_rts(ep, p, (LCI_comp_t) value);
      }
      break;
    case LCI_MSG_RTR:
      lc_pk_init(ep, -1, LC_PROTO_LONG, p);
      int tgt_rank = src_rank;

      p->context.sync = (LCI_syncl_t*) p->data.rts.ctx;
      lc_server_rma_rtr(ep->server, ep->server->rep[tgt_rank].handle,
                        (void*)p->data.rts.src_addr, p->data.rtr.tgt_addr,
                        p->data.rtr.rkey, p->data.rts.size, p->data.rtr.comm_id, p);
      break;
    case LC_PROTO_LONG:
      break;
    default:
      LCI_DBG_Assert(false, "unknown proto!");
  }
  return;

  if (proto == LC_PROTO_LONG) {
    lc_serve_recv_imm(ep, p, tag, cap);
    return;
  }

#ifdef LCI_SERVER_HAS_EXP
  if (cap & EP_AR_EXP) {
    return lc_serve_recv_match(ep, p, proto, meta, cap);
  } else
#endif
#ifdef LCI_SERVER_HAS_DYN
  if (cap & EP_AR_DYN) {
    return lc_serve_recv_dyn(ep, p, proto, meta, cap);
  } else
#endif
#ifdef LCI_SERVER_HAS_IMM
    if (cap & EP_AR_IMM) {
    return lc_serve_recv_imm(ep, p, meta, cap);
  } else
#endif
    // placeholder for anything else.
  {
  }
}

static inline void lc_serve_recv_rdma(lc_packet* p, LCI_msg_type_t proto)
{
  LCI_endpoint_t ep = LCI_ENDPOINTS[PROTO_GET_RGID(proto)];
  p->context.sync->request.tag = PROTO_GET_TAG(proto);
  lc_ce_dispatch(ep, p, p->context.sync, ep->property);
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
    case LCI_MSG_RTS:
      // sendl has not been completed locally. No need to process completion.
      LCII_free_packet(LCII_mbuffer2packet(ctx->data.mbuffer));
      break;
    case LCI_MSG_RTR:
      // recvl has not been completed locally. No need to process completion.
      LCII_free_packet(LCII_mbuffer2packet(ctx->data.mbuffer));
      break;
    default:
      LCI_DBG_Assert(false, "unexpected proto %d\n", proto);
  }
  if (proto == LCI_MSG_RTR) {
    if (--p->context.ref == 0)
      lc_ce_dispatch(ep, p, p->context.sync, ep->property);
    // Have to keep the ref counting here, otherwise there is a nasty race
    // when the RMA is done and this RTR is not.
    // Note that this messed up the order of completion.
    // If one expects MPI order, should need to split completion here.
  } else if (proto == LC_PROTO_LONG) {
    if (p->context.sync != NULL) {
      LCI_one2one_set_full(p->context.sync);
    }
    if (p->context.ref == 1) {
      lc_pk_free_data(ep, p);
    }
  } else if (proto == LCI_MSG_RTS) {
//    LCI_DBG_Assert((LCI_sync_t*)p->data.rts.ce != NULL);
//    LCI_one2one_set_full((LCI_sync_t*)p->data.rts.ce);
    LCI_DBG_Assert(p->context.ref == 1, "p->context.ref = %d\n", p->context.ref);
    lc_pk_free(ep, p);
  } else {
    LCI_DBG_Assert(proto == LC_PROTO_DATA, "unexpected proto %d\n", proto);
    if (p->context.sync != NULL)
      LCI_one2one_set_full(p->context.sync);
    if (p->context.ref == 1) {
      lc_pk_free_data(ep, p);
    }
  }
}

static inline void lc_serve_imm(lc_packet* p)
{
  LCI_endpoint_t ep = p->context.ep;
  lc_server_rma_dereg(p->context.rma_mem);
  if (--p->context.ref == 0)
    lc_ce_dispatch(ep, p, p->context.sync, ep->property);
}

#endif
