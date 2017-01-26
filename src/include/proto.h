#ifndef MV_PROTO_H
#define MV_PROTO_H

typedef struct {
  mv_am_func_t func_am;
  mv_am_func_t func_ps;
  uint8_t am_fid;
  uint8_t ps_fid;
} mv_proto_spec_t;

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

extern const mv_proto_spec_t mv_proto[11];

#define mv_set_proto(p, N)\
{\
  p->data.header.am_fid = mv_proto[N].am_fid;\
  p->data.header.ps_fid = mv_proto[N].ps_fid;\
} while (0); \

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


#include "proto/proto_eager.h"
#include "proto/proto_ext.h"
#include "proto/proto_rdz.h"

MV_INLINE
void mv_serve_recv(mvh* mv, mv_packet* p_ctx)
{
  const uint8_t fid = p_ctx->data.header.am_fid;
  ((p_ctx_handler)mv->am_table[fid])(mv, p_ctx);
}

MV_INLINE
void mv_serve_send(mvh* mv, mv_packet* p_ctx)
{
  if (!p_ctx) return;
  const uint8_t fid = p_ctx->data.header.ps_fid;
  if (fid) {
    ((p_ctx_handler)mv->am_table[fid])(mv, p_ctx);
  } else { // by default, returns to the pool.
    mv_pool_put_to(mv->pkpool, p_ctx, p_ctx->data.header.poolid);
  }
}

MV_INLINE
void mv_serve_imm(uint32_t imm)
{
  printf("GOT ID %d\n", imm);
}

#endif
