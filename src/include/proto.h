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

#define LC_PROTO_QUEUE   (0b0000001)
#define LC_PROTO_ALLOC   (0b0000010)
#define LC_PROTO_TAG     (0b0000100)

#define LC_PROTO_DATA    (0b0001000)
#define LC_PROTO_RTR     (0b0010000)
#define LC_PROTO_RTS     (0b0100000)
#define LC_PROTO_LONG    (0b1000000)

#define MAKE_PROTO(proto, tag) (((uint32_t)(proto)) | ((uint32_t)tag << 8))
#define MAKE_SIG(sig, id) (((uint32_t)(sig)) | ((uint32_t) id << 3))

#if 0
LC_INLINE
int lci_send(void* h, void* src, size_t size,
             uint8_t proto, lc_packet* p)
{
  p->context.proto = proto;
  p->context.poolid = 0;
  return lc_server_send(h, src, size, p, MAKE_PROTO(proto, 0));
}

LC_INLINE
void lci_put_ofs(lch* mv, void* src, size_t size, int rank, uintptr_t tgt,
             size_t offset, uint32_t rkey, uint32_t sig, lc_packet* p)
{
  lc_server_rma_signal(mv->server, rank, src, tgt, offset, rkey, size, sig, p);
}

LC_INLINE
void lci_put(lch* mv, void* src, size_t size, int rank, uintptr_t tgt,
             uint32_t rkey, uint32_t sig, lc_packet* p)
{
  lc_server_rma_signal_rtr(mv->server, rank, src, tgt, rkey, size, sig, p);
}

LC_INLINE
void lci_get(lch* mv, void* src, size_t size, int rank, uintptr_t tgt, size_t
             offset, uint32_t rkey, uint32_t sig, lc_packet* p)
{
  lc_server_get(mv->server, rank, src, tgt, offset, rkey, size, sig, p);
}
#endif

LC_INLINE
void lci_rdz_prepare(struct lci_ep* ep, void* src, size_t size, lc_packet* p)
{
  // p->context.req = req;
  // uintptr_t rma_mem = 0;// lc_server_rma_reg(mv->server, src, size);
  // FIXME: 0x1 is not correct, uncomment the above when ready.
  lc_server_post_rma(ep->hw.handle, src, size, 0x1);
  // p->context.rma_mem = rma_mem;
  p->data.rtr.comm_id = (uint32_t)((uintptr_t)p - ep->base_addr);
  p->data.rtr.tgt_addr = (uintptr_t)src;
  p->data.rtr.rkey = 0x1; //lc_server_rma_key(rma_mem);
}

LC_INLINE
void lc_serve_recv(struct lci_ep* ep, lc_packet* p, uint32_t proto)
{
  void* buf;
  size_t size;

  if (proto & LC_PROTO_DATA) {
    if (proto & LC_PROTO_ALLOC) {
      p->context.req->buffer = ep->alloc(p->context.req->size);
      memcpy(p->context.req->buffer, &p->data, p->context.req->size);
    } 
    if (proto & LC_PROTO_TAG) {
      p->context.proto = proto;
      const lc_key key = lc_make_key(p->context.req->rank, p->context.req->tag);
      lc_value value = (lc_value)p;
      if (!lc_hash_insert(ep->tbl, key, &value, SERVER)) {
        lc_req* req = (lc_req*) value;
        req->size = p->context.req->size;
        memcpy(req->buffer, p->data.buffer, p->context.req->size);
        req->flag = 1;
      } else {
        goto skip;
      }
    }

    if (proto & LC_PROTO_QUEUE) {
      p->context.req->flag = 1;
      cq_push(&ep->cq, p);
    } else {
      lc_pool_put(ep->pkpool, p);
    }
  } else if (proto & LC_PROTO_RTS) {
    if (proto & LC_PROTO_ALLOC) {
      size = p->data.rts.size;
      buf = ep->alloc(p->data.rts.size);
      p->context.req->buffer = buf;
    }
    if (proto & LC_PROTO_TAG) {
      p->context.proto = proto;
      const lc_key key = lc_make_key(p->context.req->rank, p->context.req->tag);
      lc_value value = (lc_value)p;
      if (!lc_hash_insert(ep->tbl, key, &value, SERVER)) {
        lc_req* req = (lc_req*) value;
        req->rank = p->context.req->rank;
        p->context.req = req;
        size = p->data.rts.size;
        buf = req->buffer;
      } else {
        goto skip;
      }
    }

    uint32_t next_proto = (proto & ~LC_PROTO_RTS) | LC_PROTO_RTR;
    lci_rdz_prepare(ep, buf, size, p);
    p->context.proto = next_proto;
    lc_server_send(ep->hw.handle, p->context.req->rank, &p->data, sizeof(struct packet_rtr), p, next_proto);
  } else if (proto & LC_PROTO_RTR) {
    p->context.proto = LC_PROTO_LONG;
    lc_server_rma_rtr(ep->hw.handle, p->context.req->rank,
        (void*) p->data.rts.src_addr, 
        p->data.rtr.tgt_addr, p->data.rtr.rkey, p->data.rts.size, 
        p->data.rtr.comm_id, p);
  }

skip:
  /** 2-sided protocol requires send/recv... thus skip next step until matched */
  return;
}

LC_INLINE
void lc_serve_send(struct lci_ep* ep, lc_packet* p, uint32_t proto)
{
  if (proto & LC_PROTO_RTR) {
    if (proto & LC_PROTO_QUEUE) {
      p->context.req->flag = 0;
      cq_push(&ep->cq, p);
    } else {
      lc_pool_put(ep->pkpool, p);
    }
  } else if (proto & LC_PROTO_LONG) {
    ((lc_req*) p->data.rts.req)->flag = 1;
    lc_pool_put(ep->pkpool, p);
  } else {
    if (!p->context.runtime) {
      if (p->context.poolid)
        lc_pool_put_to(ep->pkpool, p, p->context.poolid);
      else
        lc_pool_put(ep->pkpool, p);
    }
  }
}

LC_INLINE
void lc_serve_imm(struct lci_ep* ep, uint32_t imm)
{
  // FIXME(danghvu): This comm_id is here due to the imm
  // only takes uint32_t, if this takes uint64_t we can
  // store a pointer to this request context.
  // uint32_t type = imm & 0b0111;
  // uint32_t id = imm >> 3;
  uintptr_t addr = ep->base_addr + imm;
  lc_packet* p = (lc_packet*)addr;
  p->context.req->flag = 1;
  // lc_pool_put(ep->pkpool, p);
}
#endif
