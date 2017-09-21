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

#define LC_PROTO_QUEUE   (0b0000000)
#define LC_PROTO_TAG     (0b0000001)
#define LC_PROTO_TGT     (0b0000010)

#define LC_PROTO_DATA    (0b0000100)
#define LC_PROTO_RTR     (0b0001000)
#define LC_PROTO_RTS     (0b0010000)
#define LC_PROTO_LONG    (0b0100000)

#define MAKE_PROTO(proto, tag) (((uint32_t)proto) | ((uint32_t)tag << 8))
#define MAKE_SIG(sig, id) (((uint32_t)sig) | ((uint32_t) id << 2))

LC_INLINE
int lci_send(lch* mv, const void* src, size_t size, int rank, int tag,
             uint8_t proto, lc_packet* p)
{
  assert(tag >> 24 == 0 && "Tag is out-of-range");
  p->context.proto = proto;
  p->context.poolid = (size > 128) ? lc_pool_get_local(mv->pkpool) : 0;
  return lc_server_send(mv->server, rank, (void*)src, size, p, MAKE_PROTO(proto, tag));
}

LC_INLINE
void lci_put(lch* mv, void* src, size_t size, int rank, uintptr_t tgt,
             uint32_t rkey, uint32_t sig, lc_packet* p)
{
  lc_server_rma_signal(mv->server, rank, src, tgt, rkey, size, sig, p);
}

LC_INLINE
void lci_rdz_prepare(lch* mv, void* src, size_t size, lc_req* ctx,
                     lc_packet* p)
{
  p->context.req = ctx;
  uintptr_t rma_mem = lc_server_rma_reg(mv->server, src, size);
  p->context.rma_mem = rma_mem;
  p->data.rtr.comm_id = (uint32_t)((uintptr_t)p - (uintptr_t)lc_heap_ptr(mv));
  p->data.rtr.tgt_addr = (uintptr_t)src;
  p->data.rtr.rkey = lc_server_rma_key(rma_mem);
}

LC_INLINE
void lc_serve_recv(lch* mv, lc_packet* p, uint8_t proto)
{
  if (proto & LC_PROTO_RTR) {
    p->context.req = (lc_req*) p->data.rtr.req;
    int signal = p->context.proto & 0b11;
    p->context.proto = LC_PROTO_LONG;
    lci_put(mv, (void*) p->data.rts.src_addr, p->data.rts.size, p->context.from,
        p->data.rtr.tgt_addr, p->data.rtr.rkey,
        MAKE_SIG(signal, p->data.rtr.comm_id), p);
  } else if (proto & LC_PROTO_TAG) {
    p->context.proto = proto;
    const lc_key key = lc_make_key(p->context.from, p->context.tag);
    lc_value value = (lc_value)p;
    if (!lc_hash_insert(mv->tbl, key, &value, SERVER)) {
      lc_req* req = (lc_req*) value;
      if (proto & LC_PROTO_DATA) {
        req->size = p->context.size;
        memcpy(req->buffer, p->data.buffer, req->size);
        LC_SET_REQ_DONE_AND_SIGNAL(req);
        lc_pool_put(mv->pkpool, p);
      } else {
        req->size = p->data.rts.size;
        lci_rdz_prepare(mv, req->buffer, req->size, req, p);
        lci_send(mv, &p->data, sizeof(struct packet_rtr),
            req->rank, req->tag, LC_PROTO_RTR | LC_PROTO_TAG, p);
      }
    }
  } else if ((proto & 1) == LC_PROTO_QUEUE) {
    p->context.proto = proto;
#ifndef USE_CCQ
    dq_push_top(&mv->queue, (void*) p);
#else
    lcrq_enqueue(&mv->queue, (void*) p);
#endif
  }
}

LC_INLINE
void lc_serve_send(lch* mv, lc_packet* p, uint8_t proto)
{
  if (proto & LC_PROTO_RTR) {
    return;
  } else if (proto & LC_PROTO_LONG) {
    LC_SET_REQ_DONE_AND_SIGNAL(p->context.req);
    lc_pool_put(mv->pkpool, p);
  } else {
    if (p->context.runtime) {
      if (p->context.poolid)
        lc_pool_put_to(mv->pkpool, p, p->context.poolid);
      else
        lc_pool_put(mv->pkpool, p);
    }
  }
}

LC_INLINE
void lc_serve_imm(lch* mv, uint32_t imm)
{
  // FIXME(danghvu): This comm_id is here due to the imm
  // only takes uint32_t, if this takes uint64_t we can
  // store a pointer to this request context.
  uint32_t type = imm & 0b11;
  uint32_t id = imm >> 2;
  uintptr_t addr = (uintptr_t)lc_heap_ptr(mv) + id;
  if (type != LC_PROTO_TGT) {
    lc_packet* p = (lc_packet*)addr;
    LC_SET_REQ_DONE_AND_SIGNAL(p->context.req);
    lc_server_rma_dereg(p->context.rma_mem);
    lc_pool_put(mv->pkpool, p);
  } else {
    lc_packet* p = (lc_packet*)addr;
    if (p->context.req) LC_SET_REQ_DONE_AND_SIGNAL(p->context.req);
  }
}
#endif
