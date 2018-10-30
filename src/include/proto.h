#ifndef LC_PROTO_H
#define LC_PROTO_H

#include "debug.h"
#include "lc/hashtable.h"
#include "lc/macro.h"

/* 2-bit is enough for those thing. */
typedef enum lc_proto {
 LC_PROTO_DATA  = 0,
 LC_PROTO_RTR   = 1,
 LC_PROTO_RTS   = 2,
 LC_PROTO_LONG  = 3,
} lc_proto;

#include "server/server.h"

#define MAKE_PROTO(rgid, proto, meta)  (proto | (rgid << 2) | (meta << 16))

#define PROTO_GET_PROTO(proto) (proto         & 0b011)
#define PROTO_GET_RGID(proto)  ((proto >> 2)  & 0b01111111)
#define PROTO_GET_META(proto)  ((proto >> 16) & 0xffff)

/* CRC-32C (iSCSI) polynomial in reversed bit order. */
#define POLY 0x82f63b78
inline uint32_t crc32c(char *buf, size_t len)
{
  uint32_t crc = 0;
  int k;

  crc = ~crc;
  while (len--) {
    crc ^= *buf++;
    for (k = 0; k < 8; k++)
      crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
  }
  return ~crc;
}

static inline void lci_init_req(void* buf, size_t size, lc_req* req)
{
  req->buffer = buf;
  req->size = size;
  lc_reset((void*) &req->sync);
}

static inline void lci_prepare_rtr(struct lci_ep* ep, void* src, size_t size, lc_packet* p)
{
  uintptr_t rma_mem = lc_server_rma_reg(ep->server, src, size);
  p->context.rma_mem = rma_mem;
  p->data.rtr.comm_id = ((uintptr_t)p - (uintptr_t) lc_server_heap_ptr(ep->server));
  p->data.rtr.tgt_addr = (uintptr_t)src;
  p->data.rtr.rkey = lc_server_rma_key(rma_mem);
  dprintf("%d] post recv rdma %p %d via %d\n", lcg_rank, src, size, p->data.rtr.rkey);
}

static inline void lci_prepare_rts(void* src, size_t size, uint32_t rgid, lc_send_cb cb, void* ce, lc_packet* p)
{
  p->data.rts.src_addr = (uintptr_t) src;
  p->data.rts.size = size;
  p->data.rts.rgid = rgid;
  p->data.rts.cb = cb;
  p->data.rts.ce = (uintptr_t) ce;
}

static inline void lci_pk_free(lc_ep ep, lc_packet* p)
{
  lc_pool_put(ep->pkpool, p);
}

static inline void lci_pk_free_data(lc_ep ep, lc_packet* p)
{
  if (p->context.poolid != -1)
    lc_pool_put_to(ep->pkpool, p, p->context.poolid);
  else
    lc_pool_put(ep->pkpool, p);
}

typedef void(func_cb(struct lci_ep*, lc_packet*));

static inline void lci_ce_am(lc_ep ep, lc_packet* p)
{
  lc_req* req = p->context.req;
  ep->handler(req);
  lci_pk_free_data(ep, p);
}

static inline void lci_ce_signal(lc_ep ep __UNUSED__, lc_packet* p)
{
  lc_req* req = p->context.req;
  lc_signal((void*) &(req->sync));
  lci_pk_free_data(ep, p);
}

static inline void lci_ce_queue(lc_ep ep, lc_packet* p)
{
  lc_req* req = p->context.req;
  req->parent = p;
  req->sync = 1;
  cq_push(&ep->cq, req);
}

static inline void lci_handle_rtr(struct lci_ep* ep, lc_packet* p)
{
  lci_pk_init(ep, -1, LC_PROTO_LONG, p);
  dprintf("%d] rma %p --> %p %.4x via %d\n", lcg_rank, p->data.rts.src_addr, p->data.rtr.tgt_addr, crc32c((char*) p->data.rts.src_addr, p->data.rts.size), p->data.rtr.rkey);
  lc_server_rma_rtr(ep->server, p->context.req->rhandle,
      (void*) p->data.rts.src_addr,
      p->data.rtr.tgt_addr, p->data.rtr.rkey, p->data.rts.size,
      p->data.rtr.comm_id, p);
}

static inline void lci_handle_rts(struct lci_ep* ep, lc_packet* p)
{
  lci_pk_init(ep, -1, LC_PROTO_RTR, p);
  lc_proto proto = MAKE_PROTO(p->data.rts.rgid, LC_PROTO_RTR, 0);
  lci_prepare_rtr(ep, p->context.req->buffer, p->data.rts.size, p);
  lc_server_sendm(ep->server, p->context.req->rhandle,
      sizeof(struct packet_rtr), p, proto);
}

static inline void lci_serve_recv_imm(struct lci_ep* ep, lc_packet* p, lc_proto proto, func_cb complete)
{
  p->context.req->meta = PROTO_GET_META(proto);
  proto = PROTO_GET_PROTO(proto);
  p->context.req->buffer = &p->data;
  complete(ep, p);
}

static inline void lci_serve_recv_dyn(struct lci_ep* ep, lc_packet* p, lc_proto proto,
    func_cb complete)
{
  p->context.req->meta = PROTO_GET_META(proto);
  proto = PROTO_GET_PROTO(proto);

  if (proto == LC_PROTO_DATA) {
    void* buf = ep->alloc(p->context.req->size, &(p->context.req->ctx));
    memcpy(buf, &p->data, p->context.req->size);
    p->context.req->buffer = buf;
    complete(ep, p);
  } else if (proto == LC_PROTO_RTS) {
    void* buf = ep->alloc(p->data.rts.size, &(p->context.req->ctx));
    lci_init_req(buf, p->data.rts.size, p->context.req);
    lci_handle_rts(ep, p);
  } else if (proto == LC_PROTO_RTR) {
    lci_handle_rtr(ep, p);
  };
}

static inline void lci_serve_recv_expl(struct lci_ep* ep, lc_packet* p, lc_proto proto,
                        func_cb complete)
{
  p->context.req->meta = PROTO_GET_META(proto);
  p->context.proto = proto = PROTO_GET_PROTO(proto);

  if (proto == LC_PROTO_DATA) {
    const lc_key key = lc_make_key(p->context.req->rank, p->context.req->meta);
    lc_value value = (lc_value)p;
    if (!lc_hash_insert(ep->tbl, key, &value, SERVER)) {
      lc_req* req = (lc_req*) value;
      req->size = p->context.req->size;
      memcpy(req->buffer, p->data.buffer, p->context.req->size);
      p->context.req = req;
      complete(ep, p);
    }
  } else if (proto == LC_PROTO_RTS) {
    const lc_key key = lc_make_key(p->context.req->rank, p->context.req->meta);
    lc_value value = (lc_value)p;
    if (!lc_hash_insert(ep->tbl, key, &value, SERVER)) {
      p->context.req = (lc_req*) value;
      lci_handle_rts(ep, p);
    }
  } else if (proto == LC_PROTO_RTR) {
    lci_handle_rtr(ep, p);
  };
}

static inline void lci_serve_recv_dispatch(lc_ep ep, lc_packet* p, lc_proto proto, const long cap)
{
  if (cap & EP_AR_EXP) {
    if (cap & EP_CE_SYNC)
      return lci_serve_recv_expl(ep, p, proto, lci_ce_signal);
    if (cap & EP_CE_CQ)
      return lci_serve_recv_expl(ep, p, proto, lci_ce_queue);
    if (cap & EP_CE_AM) {
      return lci_serve_recv_expl(ep, p, proto, lci_ce_am);
    }
  } else if (cap & EP_AR_DYN) {
    if (cap & EP_CE_CQ)
      return lci_serve_recv_dyn(ep, p, proto, lci_ce_queue);
    if (cap & EP_CE_AM)
      return lci_serve_recv_dyn(ep, p, proto, lci_ce_am);
  } else if (cap & EP_AR_IMM) {
    if (cap & EP_CE_CQ)
      return lci_serve_recv_imm(ep, p, proto, lci_ce_queue);
    if (cap & EP_CE_AM)
      return lci_serve_recv_imm(ep, p, proto, lci_ce_am);
  }
}

static inline void lci_serve_recv(lc_packet* p, lc_proto proto)
{
  // NOTE: this should be RGID because it is received from remote.
  struct lci_ep* ep = lcg_ep_list[PROTO_GET_RGID(proto)];
  return lci_serve_recv_dispatch(ep, p, proto, ep->cap);
}

static inline void lci_ce_dispatch(lc_ep ep, lc_packet* p, const long cap)
{
  if (cap & EP_CE_SYNC) {
    lci_ce_signal(ep, p);
  } else if (cap & EP_CE_CQ) {
    lci_ce_queue(ep, p);
  } else if (cap & EP_CE_AM) {
    lci_ce_am(ep, p);
  } else {
    // We could do a counting completion here.
    assert(0 && "TODO");
  }
}

static inline void lci_serve_recv_rdma(lc_packet* p, lc_proto proto)
{
  struct lci_ep* ep = lcg_ep_list[PROTO_GET_RGID(proto)];
  p->context.req->meta = PROTO_GET_META(proto);
  lci_ce_dispatch(ep, p, ep->cap);
}

static inline void lci_serve_send(lc_packet* p)
{
  struct lci_ep* ep = p->context.ep;
  lc_proto proto = p->context.proto;

  if (proto == LC_PROTO_RTR) {
    // Do nothing, the data is on the way using this packet as context.
    // Note that this messed up the order of completion.
    // If one expects MPI order, should handle completion here.
  } else if (proto == LC_PROTO_LONG) {
    p->data.rts.cb((void*) p->data.rts.ce);
    lci_pk_free(ep, p);
  } else if (proto == LC_PROTO_RTS) {
    lci_pk_free(ep, p);
  } else {
    lci_pk_free_data(ep, p);
  }
}

static inline void lci_serve_imm(lc_packet* p)
{
  struct lci_ep* ep = p->context.ep;
  dprintf("%d] got %p %.4x\n", lcg_rank, p->context.req->buffer, crc32c(p->context.req->buffer, p->context.req->size));
  lci_ce_dispatch(ep, p, ep->cap);
  lc_server_rma_dereg(p->context.rma_mem);
}

#endif
