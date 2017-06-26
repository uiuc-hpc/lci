#ifndef LC_PROTO_H
#define LC_PROTO_H

#define INIT_CTX(ctx)         \
  {                           \
    ctx->buffer = (void*)src; \
    ctx->size = size;         \
    ctx->rank = rank;         \
    ctx->tag = tag;           \
    ctx->sync = 0;            \
    ctx->type = REQ_PENDING;  \
  }

#define MAKE_PROTO(proto, tag) (((uint32_t)proto) | ((uint32_t)tag << 8))
#define MAKE_SIG(sig, id) (((uint32_t)sig << 30) | id)

typedef struct {
  lc_am_func_t func_am;
  lc_am_func_t func_ps;
} lc_proto_spec_t;

const lc_proto_spec_t lc_proto[10] __attribute__((aligned(64)));

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

LC_INLINE void lci_rdz_prepare(lch* mv, void* src, int size, lc_ctx* ctx,
                               lc_packet* p)
{
  p->context.req = (uintptr_t)ctx;
  uintptr_t rma_mem = lc_server_rma_reg(mv->server, src, size);
  p->context.rma_mem = rma_mem;
  p->data.rtr.comm_id = (uint32_t)((uintptr_t)p - (uintptr_t)lc_heap_ptr(mv));
  p->data.rtr.tgt_addr = (uintptr_t)src;
  p->data.rtr.rkey = lc_server_rma_key(rma_mem);
}

LC_INLINE
void lc_serve_recv(lch* mv, lc_packet* p_ctx, uint32_t proto)
{
  lc_proto[proto].func_am(mv, p_ctx);
}

LC_INLINE
void lc_serve_send(lch* mv, lc_packet* p_ctx, uint32_t proto)
{
  const lc_am_func_t f = lc_proto[proto].func_ps;
  if (likely(f)) {
    f(mv, p_ctx);
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
    lc_ctx* req = (lc_ctx*)p->context.req;
    lc_server_rma_dereg(p->context.rma_mem);
    req->type = REQ_DONE;
    lc_pool_put(mv->pkpool, p);
  } else if (type == RMA_SIGNAL_SIMPLE) {
    struct lc_rma_ctx* ctx = (struct lc_rma_ctx*)addr;
    if (ctx->req) ((lc_ctx*)ctx->req)->type = REQ_DONE;
  } else {
    lc_packet* p = (lc_packet*)addr;
    lc_ctx* req = (lc_ctx*)p->context.req;
    lc_server_rma_dereg(p->context.rma_mem);
    const lc_key key = lc_make_key(p->context.from, p->context.tag);
    lc_value value = (lc_value)p;
    if (!lc_hash_insert(mv->tbl, key, &value, 1)) {
      req->type = REQ_DONE;
      if (req->sync) thread_signal(req->sync);
      lc_pool_put(mv->pkpool, p);
    }
  }
}
#endif
