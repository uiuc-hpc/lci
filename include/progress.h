#ifndef PROGRESS_H_
#define PROGRESS_H_

#include "mpiv.h"
#include "request.h"

typedef void (*_0_arg)(void*, uint32_t);
typedef void (*_1_arg)(void*, uint32_t, uint32_t);
typedef void (*_2_arg)(void*, uint32_t, uint32_t, uint32_t);
typedef void (*_3_arg)(void*, uint32_t, uint32_t, uint32_t, uint32_t);

void mv_post_recv(packet* p);

void mv_recv_imm(uint32_t imm) {
  printf("GOT ID %d\n", imm);
}

void mv_recv_am(packet *p) {
  uint8_t fid = (uint8_t) p->header.tag;
  uint32_t* buffer = (uint32_t*) p->content.buffer;
  uint32_t size = buffer[0];
  uint32_t count = buffer[1];
  char* data = (char*) &buffer[2 + count];
  switch (count) {
    case 0:
      ((_0_arg) MPIV.am_table[fid])(data, size);
      break;
    case 1:
      ((_1_arg) MPIV.am_table[fid])(data, size, buffer[2]);
      break;
    case 2:
      ((_2_arg) MPIV.am_table[fid])(data, size, buffer[2], buffer[3]);
      break;
    case 3:
      ((_3_arg) MPIV.am_table[fid])(data, size, buffer[2], buffer[3], buffer[4]);
      break;
  };
}

void mv_complete_rndz(packet* p, MPIV_Request* s) {
  p->header = {SEND_WRITE_FIN, 0, MPIV.me, s->tag};
  p->content.rdz.sreq = (uintptr_t)s;
  MPIV.server.write_rma(s->rank, s->buffer,
          MPIV.server.heap_lkey(), (void*)p->content.rdz.tgt_addr,
          p->content.rdz.rkey, s->size, (void*)p);
}

void mv_recv_recv_ready(packet* p) {
  mv_key key = mv_make_rdz_key(p->header.from, p->header.tag);
  mv_value value = (mv_value) p;
  if (!hash_insert(MPIV.tbl, key, &value)) {
    mv_complete_rndz(p, (MPIV_Request*) value);
  }
}

void mv_recv_send_ready_fin(packet* p_ctx) {
  // Now data is already ready in the content.buffer.
  MPIV_Request* req = (MPIV_Request*)(p_ctx->content.rdz.rreq);
  startt(signal_timing);
  startt(wake_timing);

  mv_key key = mv_make_key(req->rank, req->tag);
  mv_value value;
  if (!hash_insert(MPIV.tbl, key, &value)) {
    if (req->type != REQ_NULL) req->type = REQ_DONE;
    thread_signal(req->sync);
  }
  stopt(signal_timing);
  mv_pp_free(MPIV.pkpool, p_ctx);
}

void mv_post_recv(packet*);

void mv_recv_short(packet* p) {
  startt(misc_timing);
  const mv_key key = mv_make_key(p->header.from, p->header.tag); 
  mv_value value = (mv_value) p;
  stopt(misc_timing);

  if (!hash_insert(MPIV.tbl, key, &value)) {
    // comm-thread comes later.
    MPIV_Request* req = (MPIV_Request*) value;
    memcpy(req->buffer, p->content.buffer, req->size);
    if (req->type != REQ_NULL) req->type = REQ_DONE;
    thread_signal(req->sync);
    mv_pp_free(MPIV.pkpool, p);
  }
}

typedef void (*p_ctx_handler)(packet* p_ctx);
static p_ctx_handler* handle;

static void mv_progress_init() {
  posix_memalign((void**)&handle, 64, 64);
  handle[SEND_SHORT] = mv_recv_short;
  handle[RECV_READY] = mv_recv_recv_ready;
  handle[SEND_READY_FIN] = mv_recv_send_ready_fin;
  // AM.
  handle[SEND_AM] = mv_recv_am;
}

inline void mv_serve_recv(packet* p_ctx) {
  // packet* p_ctx = (packet*)wc.wr_id;
  const auto& type = p_ctx->header.type;
  handle[type](p_ctx);
}

inline void mv_serve_send(packet* p_ctx) {
  if (!p_ctx) return;

  const auto& type = p_ctx->header.type;
  if (type == SEND_WRITE_FIN) {
    MPIV_Request* req = (MPIV_Request*)p_ctx->content.rdz.sreq;
    p_ctx->header.type = SEND_READY_FIN;
    MPIV.server.write_send(req->rank, p_ctx,
            sizeof(packet_header) + sizeof(mv_rdz), p_ctx);
    mv_key key = mv_make_key(req->rank, (1 << 30) | req->tag);
    mv_value value;
    if (!hash_insert(MPIV.tbl, key, &value)) {
      req->type = REQ_DONE;
      thread_signal(req->sync);
    }
    stopt(rdma_timing);
  } else {
    mv_pp_free_to(MPIV.pkpool, p_ctx, p_ctx->header.poolid);
  }
}

#endif
