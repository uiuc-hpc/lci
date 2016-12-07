#ifndef MV_PROTO_H_
#define MV_PROTO_H_

extern int PROTO_SEND_WRITE_FIN;

MV_INLINE void proto_complete_rndz(mv_engine* mv, mv_packet* p, mv_ctx* s)
{
  p->header.fid = PROTO_SEND_WRITE_FIN;
  p->header.poolid = 0;
  p->header.from = mv->me;
  p->header.tag = s->tag;
  p->content.rdz.sreq = (uintptr_t)s;

  mv_server_rma(mv->server, s->rank, s->buffer, (void*)p->content.rdz.tgt_addr,
                p->content.rdz.rkey, s->size, (void*)p);
}


void mv_send_rdz_post(mv_engine* mv, mv_ctx* ctx, mv_sync* sync);
void mv_send_rdz_init(mv_engine* mv, mv_ctx* ctx);
void mv_send_rdz(mv_engine* mv, mv_ctx* ctx, mv_sync* sync);

void mv_send_eager(mv_engine* mv, mv_ctx* ctx);
void mv_recv_rdz_init(mv_engine* mv, mv_ctx* ctx);
void mv_recv_rdz_post(mv_engine* mv, mv_ctx* ctx, mv_sync* sync);
void mv_recv_rdz(mv_engine* mv, mv_ctx* ctx, mv_sync* sync);
void mv_recv_eager_post(mv_engine* mv, mv_ctx* ctx, mv_sync* sync);
void mv_recv_eager(mv_engine* mv, mv_ctx* ctx, mv_sync* sync);
void mv_am_eager(mv_engine* mv, int node, void* src, int size,
                           uint32_t fid);

void mv_put(mv_engine* mv, int node, void* dst, void* src, int size,
                      uint32_t sid);

#if 0
MV_INLINE void mv_am_eager(mv_engine* mv, int node, void* src, int size,
                           uint32_t fid)
{
  mv_packet* p = (mv_packet*) mv_pool_get(mv->pkpool); 
  p->header.fid = PROTO_AM;
  p->header.from = mv->me;
  p->header.tag = fid;
  uint32_t* buffer = (uint32_t*)p->content.buffer;
  buffer[0] = size;
  memcpy((void*)&buffer[1], src, size);
  mv_server_send(mv->server, node, p,
                 sizeof(uint32_t) + (uint32_t)size + sizeof(packet_header),
                 p);
}

MV_INLINE void mv_put(mv_engine* mv, int node, void* dst, void* src, int size,
                      uint32_t sid)
{
  mv_server_rma_signal(mv->server, node, src, dst,
                       mv_server_heap_rkey(mv->server, node), size, sid, 0);
}
#endif

#endif
