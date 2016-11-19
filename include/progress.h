#ifndef PROGRESS_H_
#define PROGRESS_H_

#include "mv.h"
#include "request.h"

typedef void (*_0_arg)(void*, uint32_t);
typedef void (*_1_arg)(void*, uint32_t, uint32_t);
typedef void (*_2_arg)(void*, uint32_t, uint32_t, uint32_t);
typedef void (*_3_arg)(void*, uint32_t, uint32_t, uint32_t, uint32_t);

MV_INLINE void proto_complete_rndz(mv_engine* mv, packet* p, MPIV_Request* s);

inline void mv_serve_imm(uint32_t imm) {
  printf("GOT ID %d\n", imm);
}

inline void mv_recv_am(mv_engine* mv, packet *p) {
  uint8_t fid = (uint8_t) p->header.tag;
  uint32_t* buffer = (uint32_t*) p->content.buffer;
  uint32_t size = buffer[0];
  uint32_t count = buffer[1];
  char* data = (char*) &buffer[2 + count];
  switch (count) {
    case 0:
      ((_0_arg) mv->am_table[fid])(data, size);
      break;
    case 1:
      ((_1_arg) mv->am_table[fid])(data, size, buffer[2]);
      break;
    case 2:
      ((_2_arg) mv->am_table[fid])(data, size, buffer[2], buffer[3]);
      break;
    case 3:
      ((_3_arg) mv->am_table[fid])(data, size, buffer[2], buffer[3], buffer[4]);
      break;
  };
}

inline void mv_recv_recv_ready(mv_engine* mv, packet* p) {
  mv_key key = mv_make_rdz_key(p->header.from, p->header.tag);
  mv_value value = (mv_value) p;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    proto_complete_rndz(mv, p, (MPIV_Request*) value);
  }
}

inline void mv_recv_send_ready_fin(mv_engine* mv, packet* p_ctx) {
  // Now data is already ready in the content.buffer.
  MPIV_Request* req = (MPIV_Request*)(p_ctx->content.rdz.rreq);
  startt(signal_timing);
  startt(wake_timing);

  mv_key key = mv_make_key(req->rank, req->tag);
  mv_value value = 0;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    if (req->type != REQ_NULL) req->type = REQ_DONE;
    thread_signal(req->sync);
  }
  stopt(signal_timing);
  mv_pp_free(mv->pkpool, p_ctx);
}

inline void mv_recv_short(mv_engine* mv, packet* p) {
  startt(misc_timing);
  const mv_key key = mv_make_key(p->header.from, p->header.tag); 
  mv_value value = (mv_value) p;
  stopt(misc_timing);

  if (!mv_hash_insert(mv->tbl, key, &value)) {
    // comm-thread comes later.
    MPIV_Request* req = (MPIV_Request*) value;
    memcpy(req->buffer, p->content.buffer, req->size);
    if (req->type != REQ_NULL) req->type = REQ_DONE;
    thread_signal(req->sync);
    mv_pp_free(mv->pkpool, p);
  }
}

typedef void (*p_ctx_handler)(mv_engine*, packet* p_ctx);
static p_ctx_handler* handle;

static inline void mv_progress_init() {
  posix_memalign((void**)&handle, 64, 64);
  handle[SEND_SHORT] = mv_recv_short;
  handle[RECV_READY] = mv_recv_recv_ready;
  handle[SEND_READY_FIN] = mv_recv_send_ready_fin;
  // AM.
  handle[SEND_AM] = mv_recv_am;
}

inline void mv_serve_recv(mv_engine* mv, packet* p_ctx) {
  // packet* p_ctx = (packet*)wc.wr_id;
  const auto& type = p_ctx->header.type;
  handle[type](mv, p_ctx);
}

inline void mv_serve_send(mv_engine* mv, packet* p_ctx) {
  if (!p_ctx) return;

  const auto& type = p_ctx->header.type;
  if (type == SEND_WRITE_FIN) {
    MPIV_Request* req = (MPIV_Request*)p_ctx->content.rdz.sreq;
    p_ctx->header.type = SEND_READY_FIN;
    mv_server_send(mv->server, req->rank, p_ctx,
            sizeof(packet_header) + sizeof(mv_rdz), p_ctx);
    mv_key key = mv_make_key(req->rank, (1 << 30) | req->tag);
    mv_value value = 0;
    if (!mv_hash_insert(mv->tbl, key, &value)) {
      req->type = REQ_DONE;
      thread_signal(req->sync);
    }
    stopt(rdma_timing);
  } else {
    mv_pp_free_to(mv->pkpool, p_ctx, p_ctx->header.poolid);
  }
}

#endif
