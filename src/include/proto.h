#ifndef MV_PROTO_H
#define MV_PROTO_H

#define INIT_CTX(ctx)        \
  {                          \
    ctx->buffer = (void*)src;\
    ctx->size = size;        \
    ctx->rank = rank;        \
    ctx->tag = tag;          \
    ctx->type = REQ_PENDING; \
  }

typedef struct {
  mv_am_func_t func_am;
  mv_am_func_t func_ps;
} mv_proto_spec_t;

// Keep this order, or change mv_proto.
enum mv_proto_name {
  MV_PROTO_NULL = 0,
  MV_PROTO_SHORT_MATCH,
  MV_PROTO_SHORT_WAIT,
  MV_PROTO_RTR_MATCH,
  MV_PROTO_LONG_MATCH,

  MV_PROTO_GENERIC,

  MV_PROTO_SHORT_ENQUEUE,
  MV_PROTO_RTS_ENQUEUE,
  MV_PROTO_RTR_ENQUEUE,
  MV_PROTO_LONG_ENQUEUE,
};
const mv_proto_spec_t mv_proto[10] __attribute__((aligned(64)));

#define mv_set_proto(p, N)    \
  {                           \
    p->data.header.proto = N; \
  }

MV_INLINE
int mvi_am_generic(mvh* mv, int node, const void* src, int size, int tag,
                   const enum mv_proto_name proto, mv_packet* p)
{
  mv_set_proto(p, proto);
  p->context.poolid = mv_pool_get_local(mv->pkpool);
  p->data.header.from = mv->me;
  p->data.header.tag = tag;
  p->data.header.size = size;
  memcpy(p->data.content.buffer, src, size);
  return mv_server_send(mv->server, node, &p->data,
                        (size_t)(size + sizeof(packet_header)), &p->context);
}

MV_INLINE
int mvi_am_rdz_generic(mvh* mv, int node, int tag, int size,
                       const enum mv_proto_name proto, mv_packet* p)
{
  mv_set_proto(p, proto);
  p->context.poolid = mv_pool_get_local(mv->pkpool);
  p->data.header.from = mv->me;
  p->data.header.tag = tag;
  p->data.header.size = size;
  return mv_server_send(mv->server, node, &p->data,
                        (size_t)(sizeof(struct mv_rdz) + sizeof(packet_header)),
                        &p->context);
}

#if 0
MV_INLINE
int mvi_am_generic2(mvh* mv, int node, void* src, int size, int tag,
                    uint8_t am_fid, uint8_t ps_fid, mv_packet* p)
{
  p->data.header.am_fid = am_fid;
  p->data.header.ps_fid = ps_fid;
  p->context.poolid = mv_pool_get_local(mv->pkpool);
  p->data.header.from = mv->me;
  p->data.header.tag = tag;
  p->data.header.size = size;
  memcpy(p->data.content.buffer, src, size);
  return mv_server_send(mv->server, node, &p->data,
                        (size_t)(size + sizeof(packet_header)), &p->context);
}
#endif

#include "proto/proto_eager.h"
#include "proto/proto_ext.h"
#include "proto/proto_rdz.h"

MV_INLINE
void mv_serve_recv(mvh* mv, mv_packet* p_ctx)
{
  enum mv_proto_name proto = p_ctx->data.header.proto;
  mv_proto[proto].func_am(mv, p_ctx);
}

MV_INLINE
void mv_serve_send(mvh* mv, mv_packet* p_ctx)
{
  if (!p_ctx) return;
  enum mv_proto_name proto = p_ctx->data.header.proto;
  if (mv_proto[proto].func_ps)
    mv_proto[proto].func_ps(mv, p_ctx);
  else
    mv_pool_put_to(mv->pkpool, p_ctx, p_ctx->context.poolid);
}

MV_INLINE
void mv_serve_imm(mvh* mv, uint32_t imm) {
  // FIXME(danghvu): This comm_id is here due to the imm
  // only takes uint32_t, if this takes uint64_t we can
  // store a pointer to this request context.
  uint64_t real_imm = (imm & (MAX_COMM_ID - 1));
  if (real_imm == imm) {
    // Match + Signal
    mv_ctx* req = (mv_ctx*) mv_comm_id[imm];
    mv_pool_put_to(mv->pkpool, req->packet, req->packet->context.poolid);
    mv_key key = mv_make_key(req->rank, req->tag);
    mv_value value = 0;
    if (!mv_hash_insert(mv->tbl, key, &value)) {
      req->type = REQ_DONE;
      if (req->sync) thread_signal(req->sync);
    }
  } else {
    // Enqueue.
    mv_packet* p = (mv_packet*) mv_comm_id[real_imm];
#ifndef USE_CCQ
    dq_push_top(&mv->queue, (void*) p);
#else
    lcrq_enqueue(&mv->queue, (void*) p);
#endif
  }
}
#endif
