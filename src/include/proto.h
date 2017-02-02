#ifndef MV_PROTO_H
#define MV_PROTO_H

#define INIT_CTX(ctx)        \
  {                          \
    ctx->buffer = src;       \
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
  MV_PROTO_SHORT,
  MV_PROTO_SHORT_WAIT,

  MV_PROTO_RECV_READY,
  MV_PROTO_READY_FIN,
  MV_PROTO_SEND_FIN,

  MV_PROTO_GENERIC,

  MV_PROTO_SHORT_ENQUEUE,
  MV_PROTO_RTS,
  MV_PROTO_RTR,
  MV_PROTO_LONG_ENQUEUE,
};
const mv_proto_spec_t mv_proto[11] __attribute__((aligned(64)));

#define mv_set_proto(p, N)    \
  {                           \
    p->data.header.proto = N; \
  }

MV_INLINE
int mvi_am_generic(mvh* mv, int node, void* src, int size, int tag,
                   const enum mv_proto_name proto, mv_packet* p)
{
  mv_set_proto(p, proto);
  p->data.header.poolid = mv_pool_get_local(mv->pkpool);
  p->data.header.from = mv->me;
  p->data.header.tag = tag;
  p->data.header.size = size;
  memcpy(p->data.content.buffer, src, size);
  return mv_server_send(mv->server, node, &p->data,
                        (size_t)(size + sizeof(packet_header)), &p->context);
}

#if 0
MV_INLINE
int mvi_am_generic2(mvh* mv, int node, void* src, int size, int tag,
                    uint8_t am_fid, uint8_t ps_fid, mv_packet* p)
{
  p->data.header.am_fid = am_fid;
  p->data.header.ps_fid = ps_fid;
  p->data.header.poolid = mv_pool_get_local(mv->pkpool);
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
    mv_pool_put_to(mv->pkpool, p_ctx, p_ctx->data.header.poolid);
}

MV_INLINE
void mv_serve_imm(uint32_t imm) { printf("GOT ID %d\n", imm); }
#endif
