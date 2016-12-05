#ifndef MV_PROTO_H_
#define MV_PROTO_H_

MV_INLINE void proto_complete_rndz(mv_engine* mv, packet* p, mv_ctx* s)
{
  p->header = {PROTO_SEND_WRITE_FIN, 0, mv->me, s->tag};
  p->content.rdz.sreq = (uintptr_t)s;
  mv_server_rma(mv->server, s->rank, s->buffer, (void*)p->content.rdz.tgt_addr,
                p->content.rdz.rkey, s->size, (void*)p);
}

MV_INLINE void mv_send_rdz_post(mv_engine* mv, mv_ctx* ctx, mv_sync* sync)
{
  ctx->sync = sync;
  ctx->type = REQ_PENDING;
  mv_key key = mv_make_key(ctx->rank, (1 << 30) | ctx->tag);
  mv_value value = (mv_value) ctx;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    ctx->type = REQ_DONE;
  }
}

MV_INLINE void mv_send_rdz_init(mv_engine* mv, mv_ctx* ctx)
{
  mv_key key = mv_make_rdz_key(ctx->rank, ctx->tag);
  mv_value value = (mv_value)ctx;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    proto_complete_rndz(mv, (packet*)value, ctx);
  }
}

MV_INLINE void mv_send_rdz(mv_engine* mv, mv_ctx* ctx, mv_sync* sync)
{
  mv_send_rdz_init(mv, ctx);
  mv_send_rdz_post(mv, ctx, sync);
  while (ctx->type != REQ_DONE) {
    thread_wait(sync);
  }
}


MV_INLINE void mv_send_eager(mv_engine* mv, mv_ctx* ctx)
{
  // Get from my pool.
  packet* p = (packet*) mv_pool_get(mv->pkpool);
  const int8_t pid = mv_pool_get_local(mv->pkpool);
  p->header = {PROTO_SHORT, pid, mv->me, ctx->tag};

  // This is a eager message, we send them immediately and do not yield
  // or create a request for it.
  // Copy the buffer.
  memcpy(p->content.buffer, ctx->buffer, ctx->size);

  mv_server_send(mv->server, ctx->rank, (void*)p,
                 (size_t)(std::max((int) 8, ctx->size) + sizeof(packet_header)), (void*)(p));
}

MV_INLINE void mv_recv_rdz_init(mv_engine* mv, mv_ctx* ctx)
{
  packet* p = (packet*) mv_pool_get(mv->pkpool); //, 0);
  p->header = {PROTO_RECV_READY, 0, mv->me, ctx->tag};
  p->content.rdz = {0, (uintptr_t)ctx, (uintptr_t)ctx->buffer,
                    mv_server_heap_rkey(mv->server)};
  mv_server_send(mv->server, ctx->rank, p, sizeof(packet_header) + sizeof(mv_rdz),
                 p);
}

MV_INLINE void mv_recv_rdz_post(mv_engine* mv, mv_ctx* ctx, mv_sync* sync)
{
  ctx->sync = sync;
  ctx->type = REQ_PENDING;
  mv_key key = mv_make_key(ctx->rank, ctx->tag);
  mv_value value = 0;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    ctx->type = REQ_DONE;
  }
}

MV_INLINE void mv_recv_rdz(mv_engine* mv, mv_ctx* ctx, mv_sync* sync)
{
  mv_recv_rdz_init(mv, ctx);
  mv_recv_rdz_post(mv, ctx, sync);
  while (ctx->type != REQ_DONE) {
    thread_wait(sync);
  }
}

MV_INLINE void mv_recv_eager_post(mv_engine* mv, mv_ctx* ctx, mv_sync* sync)
{
  ctx->sync = sync;
  ctx->type = REQ_PENDING;
  mv_key key = mv_make_key(ctx->rank, ctx->tag);
  mv_value value = (mv_value)ctx;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    ctx->type = REQ_DONE;
    packet* p_ctx = (packet*)value;
    memcpy(ctx->buffer, p_ctx->content.buffer, ctx->size);
    mv_pool_put(mv->pkpool, p_ctx);
  }
}

MV_INLINE void mv_recv_eager(mv_engine* mv, mv_ctx* ctx, mv_sync* sync)
{
  mv_recv_eager_post(mv, ctx, sync);
  while (ctx->type != REQ_DONE) {
    thread_wait(sync);
  }
}

MV_INLINE void mv_am_eager(mv_engine* mv, int node, void* src, int size,
                           uint32_t fid)
{
  packet* p = (packet*) mv_pool_get(mv->pkpool); 
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

MV_INLINE uint8_t mv_am_register(mv_engine* mv, mv_am_func_t f)
{
  MPI_Barrier(MPI_COMM_WORLD);
  mv->am_table.push_back(f);
  MPI_Barrier(MPI_COMM_WORLD);
  return mv->am_table.size() - 1;
}

#endif
