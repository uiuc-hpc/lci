#ifndef LC_PROTO_H
#define LC_PROTO_H

#include "lc/hashtable.h"
#include "lc/macro.h"
#include "server/server.h"

#define INIT_CTX(ctx)         \
  {                           \
    ctx->buffer = (void*)src; \
    ctx->size = size;         \
    ctx->flag = LC_REQ_PEND;  \
  }

/* 2-bit is enough for those thing. */
#define LC_PROTO_DATA    0
#define LC_PROTO_RTR     1
#define LC_PROTO_RTS     2
#define LC_PROTO_LONG    3

#define MAKE_PROTO(rgid, proto, meta)  (proto | (rgid << 2) | (meta << 16))

#define PROTO_GET_PROTO(proto) (proto         & 0b011)
#define PROTO_GET_RGID(proto)  ((proto >> 2)  & 0b01111111)
#define PROTO_GET_META(proto)  ((proto >> 16) & 0xffff)

#define MAKE_SIG(sig, id) (((uint32_t)(sig)) | ((uint32_t) id << 3))

LC_INLINE
void lci_rdz_prepare(struct lci_ep* ep, void* src, size_t size, lc_packet* p)
{
  // p->context.req = req;
  uintptr_t rma_mem = lc_server_rma_reg(ep->handle, src, size);
  // FIXME: 0x1 is not correct, uncomment the above when ready.
  // lc_server_post_rma(ep->dev->handle, src, size, 0x1);
  p->context.rma_mem = rma_mem;
  p->data.rtr.comm_id = ((uintptr_t)p - ep->dev->base_addr);
  p->data.rtr.tgt_addr = (uintptr_t)src;
  p->data.rtr.rkey = lc_server_rma_key(rma_mem);
}

typedef void(func_cb(struct lci_ep*, void*));

static inline void set_flag(struct lci_ep* ep, void* arg)
{
  lc_req* req = (lc_req*) arg;
  req->flag = 1;
}

static inline void queue(struct lci_ep* ep, void* arg)
{
  lc_packet* p = (lc_packet*) arg;
  p->context.req->flag = 1;
  cq_push(&ep->cq, p);
}

LC_INLINE
void lc_serve_recv_alloc(struct lci_ep* ep, lc_packet* p, uint32_t proto, func_cb complete)
{
  void* buf;
  size_t size;
  p->context.req->meta.val = PROTO_GET_META(proto);
  proto = PROTO_GET_PROTO(proto);

  if (proto == LC_PROTO_DATA) {
      p->context.req->buffer = ep->alloc(ep->ctx, p->context.req->size);
      memcpy(p->context.req->buffer, &p->data, p->context.req->size);
      complete(ep, p);
  } else if (proto == LC_PROTO_RTS) {
      size = p->data.rts.size;
      buf = ep->alloc(ep->ctx, p->data.rts.size);
      p->context.req->buffer = buf;
      p->context.ep = ep;
      p->context.req->flag = 0;
      lci_rdz_prepare(ep, buf, size, p);
      proto = MAKE_PROTO(p->data.rts.rgid, LC_PROTO_RTR, 0);
      lc_server_send(ep->handle, ep, p->context.req->rhandle, &p->data, sizeof(struct packet_rtr), p, proto);
  } else if (proto == LC_PROTO_RTR) {
      p->context.proto = MAKE_PROTO(p->data.rts.rgid, LC_PROTO_LONG, 0);
      p->context.ep = ep;
      lc_server_rma_rtr(ep->handle, p->context.req->rhandle,
          (void*) p->data.rts.src_addr,
          p->data.rtr.tgt_addr, p->data.rtr.rkey, p->data.rts.size,
          p->data.rtr.comm_id, p);
  };
}

LC_INLINE
void lc_serve_recv_expl(struct lci_ep* ep, lc_packet* p, uint32_t proto,
                        func_cb complete)
{
  void* buf;
  size_t size;
  p->context.req->meta.val = PROTO_GET_META(proto);
  proto = PROTO_GET_PROTO(proto);

  if (proto == LC_PROTO_DATA) {
      p->context.proto = proto;
      const lc_key key = lc_make_key(p->context.req->rank, p->context.req->meta.val);
      lc_value value = (lc_value)p;
      if (!lc_hash_insert(ep->tbl, key, &value, SERVER)) {
        lc_req* req = (lc_req*) value;
        req->size = p->context.req->size;
        memcpy(req->buffer, p->data.buffer, p->context.req->size);
        complete(ep, req);
        lc_pool_put(ep->pkpool, p);
      }
  } else if (proto == LC_PROTO_RTS) {
      p->context.proto = proto;
      const lc_key key = lc_make_key(p->context.req->rank, p->context.req->meta.val);
      lc_value value = (lc_value)p;
      if (!lc_hash_insert(ep->tbl, key, &value, SERVER)) {
        lc_req* req = (lc_req*) value;
        req->rank = p->context.req->rank;
        p->context.req = req;
        p->context.ep = ep;
        size = p->data.rts.size;
        buf = req->buffer;
        lci_rdz_prepare(ep, buf, size, p);
        proto = MAKE_PROTO(p->data.rts.rgid, LC_PROTO_RTR, 0);
        lc_server_send(ep->handle, ep, p->context.req->rhandle, &p->data, sizeof(struct packet_rtr), p, proto);
      }
  } else if (proto == LC_PROTO_RTR) {
      p->context.proto = MAKE_PROTO(p->data.rts.rgid, LC_PROTO_LONG, 0);
      p->context.ep = ep;
      lc_server_rma_rtr(ep->handle, p->context.req->rhandle,
          (void*) p->data.rts.src_addr,
          p->data.rtr.tgt_addr, p->data.rtr.rkey, p->data.rts.size,
          p->data.rtr.comm_id, p);
  };
}

LC_INLINE
void lc_serve_recv_any(lc_ep ep, lc_packet* p, uint32_t proto, const long server_cap)
{
  if (server_cap & EP_AR_EXPL) {
    if (server_cap & EP_CE_FLAG)
      return lc_serve_recv_expl(ep, p, proto, set_flag);
  } else if (server_cap & EP_AR_ALLOC) {
    if (server_cap & EP_CE_CQ)
      return lc_serve_recv_alloc(ep, p, proto, queue);
  }
}

LC_INLINE
void lc_serve_recv(lc_packet* p, uint32_t proto, const long server_cap)
{
  // NOTE: this should be RGID because it is received from remote.
  struct lci_ep* ep = lcg_ep_list[PROTO_GET_RGID(proto)];
  if (!server_cap) {
    return lc_serve_recv_any(ep, p, proto, ep->cap);
  }

  if (server_cap & EP_AR_EXPL) {
    if (server_cap & EP_CE_FLAG)
      return lc_serve_recv_expl(ep, p, proto, set_flag);
  } else if (server_cap & EP_AR_ALLOC) {
    if (server_cap & EP_CE_CQ)
      return lc_serve_recv_alloc(ep, p, proto, queue);
  }
}

LC_INLINE
void lc_serve_send(lc_packet* p, uint32_t proto)
{
  struct lci_ep* ep = p->context.ep;
  proto = PROTO_GET_PROTO(proto);

  if (proto == LC_PROTO_RTR) {
    if (ep->cap & EP_CE_CQ) {
      cq_push(&ep->cq, p);
    } else {
      lc_pool_put(ep->pkpool, p);
    }
  } else if (proto == LC_PROTO_LONG) {
    ((lc_req*) p->data.rts.req)->flag = 1;
    lc_pool_put(ep->pkpool, p);
  } else {
    if (p->context.poolid != -1)
      lc_pool_put_to(ep->pkpool, p, p->context.poolid);
    else
      lc_pool_put(ep->pkpool, p);
  }
}

LC_INLINE
void lc_serve_imm(lc_packet* p)
{
  // FIXME(danghvu): This comm_id is here due to the imm
  // only takes uint32_t, if this takes uint64_t we can
  // store a pointer to this request context.
  // uint32_t type = imm & 0b0111;
  // uint32_t id = imm >> 3;
  p->context.req->flag = 1;
  lc_server_rma_dereg(p->context.rma_mem);
}
#endif
