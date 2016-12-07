#include "mv.h"
#include "mv-inl.h"

void mv_serve_recv(mv_engine* mv, mv_packet* p_ctx)
{
  const int8_t fid = p_ctx->header.fid;
  ((p_ctx_handler)mv->am_table[fid])(mv, p_ctx);
}

void mv_serve_send(mv_engine* mv, mv_packet* p_ctx)
{
  if (!p_ctx) return;

  const int8_t fid = p_ctx->header.fid;
  if (unlikely(fid == PROTO_SEND_WRITE_FIN)) {
    mv_ctx* req = (mv_ctx*)p_ctx->content.rdz.sreq;
    p_ctx->header.fid = PROTO_READY_FIN;
    mv_server_send(mv->server, req->rank, p_ctx,
                   sizeof(packet_header) + sizeof(struct mv_rdz), p_ctx);
    mv_key key = mv_make_key(req->rank, (1 << 30) | req->tag);
    mv_value value = 0;
    if (!mv_hash_insert(mv->tbl, key, &value)) {
      req->type = REQ_DONE;
      thread_signal(req->sync);
    }
  } else {
    // NOTE: This improves performance on memcpy, since it sends back
    // the packet to the sender thread. However, this causes a cache misses on the
    // spinlock of the pool. TODO(danghvu): send back only medium msg ?
    mv_pool_put_to(mv->pkpool, p_ctx, p_ctx->header.poolid);
  }
}

void mv_serve_imm(uint32_t imm) { 
  printf("GOT ID %d\n", imm);
}
