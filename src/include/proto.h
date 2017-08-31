#ifndef LC_PROTO_H
#define LC_PROTO_H

#include "lc/hashtable.h"
#include "lc/macro.h"

#define INIT_CTX(ctx)         \
  {                           \
    ctx->buffer = (void*)src; \
    ctx->size = size;         \
    ctx->rank = rank;         \
    ctx->tag = tag;           \
    ctx->sync = 0;            \
    ctx->type = LC_REQ_PEND;  \
  }

#define LC_PROTO_TAG     (1<<0)
#define LC_PROTO_SHORT   (1<<1)
#define LC_PROTO_RTS     (1<<2)
#define LC_PROTO_RTR     (1<<3)
#define LC_PROTO_LONG    (1<<4)

#define MAKE_PROTO(proto, tag) (((uint32_t)proto) | ((uint32_t)tag << 8))
#define MAKE_SIG(sig, id) (((uint32_t)sig << 30) | id)

LC_INLINE
int lci_send(lch* mv, const void* src, int size, int rank, int tag,
             lc_packet* p)
{
  p->context.poolid = (size > 128) ? lc_pool_get_local(mv->pkpool) : 0;
  return lc_server_send(mv->server, rank, (void*)src, size, p,
                        MAKE_PROTO(p->context.proto, tag));
}

LC_INLINE
void lci_put(lch* mv, void* src, int size, int rank, uintptr_t tgt,
             uint32_t rkey, uint32_t type, uint32_t id, lc_packet* p)
{
  lc_server_rma_signal(mv->server, rank, src, tgt, rkey, size,
                       MAKE_SIG(type, id), p);
}

LC_INLINE
void lci_rdz_prepare(lch* mv, void* src, int size, lc_req* ctx,
                     lc_packet* p)
{
  p->context.req = (uintptr_t)ctx;
  uintptr_t rma_mem = lc_server_rma_reg(mv->server, src, size);
  p->context.rma_mem = rma_mem;
  p->data.rtr.comm_id = (uint32_t)((uintptr_t)p - (uintptr_t)lc_heap_ptr(mv));
  p->data.rtr.tgt_addr = (uintptr_t)src;
  p->data.rtr.rkey = lc_server_rma_key(rma_mem);
}

static void lc_recv_short(lch* mv, lc_packet* p)
{
  const lc_key key = lc_make_key(p->context.from, p->context.tag);
  lc_value value = (lc_value)p;
  if (p->context.proto & LC_PROTO_TAG) {
    if (!lc_hash_insert(mv->tbl, key, &value, SERVER)) {
      lc_req* req = (lc_req*)value;
      if (p->context.proto & LC_PROTO_SHORT) {
        memcpy(req->buffer, p->data.buffer, p->context.size);
        LC_SET_REQ_DONE_AND_SIGNAL(req);
        lc_pool_put(mv->pkpool, p);
      } else {
        p->context.proto = LC_PROTO_TAG | LC_PROTO_RTR;
        lci_rdz_prepare(mv, req->buffer, req->size, req, p);
        lci_send(mv, &p->data, sizeof(struct packet_rtr),
            req->rank, req->tag, p);
      }
    }
  } else {
#ifndef USE_CCQ
    dq_push_top(&mv->queue, (void*) p);
#else
    lcrq_enqueue(&mv->queue, (void*) p);
#endif
  }
}

static void lc_recv_rtr(lch* mv, lc_packet* p)
{
  lc_req* ctx = (lc_req*) p->data.rtr.sreq;
  p->context.req = (uintptr_t)ctx;
  int signal = RMA_SIGNAL_QUEUE;
  if (p->context.proto & LC_PROTO_TAG)
    signal = RMA_SIGNAL_TAG;
  p->context.proto = LC_PROTO_LONG;
  lci_put(mv, ctx->buffer, ctx->size, p->context.from,
      p->data.rtr.tgt_addr, p->data.rtr.rkey,
      signal, p->data.rtr.comm_id, p);
}

#if 0
static void lc_sent_put(lch* mv, lc_packet* p_ctx)
{
  lc_req* req = (lc_req*) p_ctx->context.req;
  LC_SET_REQ_DONE_AND_SIGNAL(req);
  lc_pool_put(mv->pkpool, p_ctx);
}
#endif

LC_INLINE
void lc_serve_recv(lch* mv, lc_packet* p, uint32_t proto)
{
  if (proto & LC_PROTO_RTR) {
    lc_recv_rtr(mv, p);
  } else {
    p->context.proto = proto;
    lc_recv_short(mv, p);
  }
}

LC_INLINE
void lc_serve_send(lch* mv, lc_packet* p_ctx, uint32_t proto)
{
  if (proto & LC_PROTO_SHORT || proto & LC_PROTO_RTS) {
    if (p_ctx->context.runtime) {
      if (p_ctx->context.poolid)
        lc_pool_put_to(mv->pkpool, p_ctx, p_ctx->context.poolid);
      else
        lc_pool_put(mv->pkpool, p_ctx);
    }
  } else if (proto & LC_PROTO_LONG) {
    lc_req* req = (lc_req*) p_ctx->context.req;
    LC_SET_REQ_DONE_AND_SIGNAL(req);
    lc_pool_put(mv->pkpool, p_ctx);
  }
}

LC_INLINE
void lc_serve_imm(lch* mv, uint32_t imm)
{
  // FIXME(danghvu): This comm_id is here due to the imm
  // only takes uint32_t, if this takes uint64_t we can
  // store a pointer to this request context.
  uint32_t type = imm >> 30;
  uint32_t id = imm & 0x0fffffff;
  uintptr_t addr = (uintptr_t)lc_heap_ptr(mv) + id;
  if (type == RMA_SIGNAL_QUEUE) {
    lc_packet* p = (lc_packet*)addr;
    lc_req* req = (lc_req*)p->context.req;
    lc_server_rma_dereg(p->context.rma_mem);
    req->type = LC_REQ_DONE;
    lc_pool_put(mv->pkpool, p);
  } else if (type == RMA_SIGNAL_SIMPLE) {
    struct lc_rma_ctx* ctx = (struct lc_rma_ctx*)addr;
    if (ctx->req) ((lc_req*)ctx->req)->type = LC_REQ_DONE;
  } else {  // match.
    lc_packet* p = (lc_packet*)addr;
    lc_req* req = (lc_req*)p->context.req;
    lc_server_rma_dereg(p->context.rma_mem);
    LC_SET_REQ_DONE_AND_SIGNAL(req);
    lc_pool_put(mv->pkpool, p);
  }
}
#endif
