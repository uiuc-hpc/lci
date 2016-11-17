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
  uint8_t fid = (uint8_t) p->header().tag;
  uint32_t* buffer = (uint32_t*) p->buffer();
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
  p->set_header(SEND_READY_FIN, MPIV.me, s->tag);
  p->set_sreq((uintptr_t)s);
  MPIV.server.write_rma(s->rank, s->buffer, MPIV.server.heap_lkey(), (void*)p->rdz_tgt_addr(),
                        p->rdz_rkey(), s->size, (void*)p);
}

void mv_recv_recv_ready(packet* p) {
  mv_key key = p->get_rdz_key();
  mv_value value = (mv_value) p;
  if (!hash_insert(MPIV.tbl, key, value)) {
    mv_complete_rndz(p, (MPIV_Request*) value);
  }
}

void mv_recv_send_ready_fin(packet* p_ctx) {
  // Now data is already ready in the buffer.
  MPIV_Request* req = (MPIV_Request*)(p_ctx->rdz_rreq());
  startt(signal_timing);
  startt(wake_timing);

  mv_key key = mv_make_key(req->rank, req->tag);
  mv_value value;
  if (!hash_insert(MPIV.tbl, key, value)) {
    req->type = REQ_DONE;
    thread_signal(req->sync);
  }
  stopt(signal_timing);
  mv_pp_free(MPIV.pkpool, p_ctx);
}

void mv_post_recv(packet*);

void mv_recv_short(packet* p) {
  startt(misc_timing);
  const mv_key key = p->get_key();
  mv_value value = (mv_value) p;
  stopt(misc_timing);

  if (!hash_insert(MPIV.tbl, key, value)) {
    // comm-thread comes later.
    MPIV_Request* req = (MPIV_Request*) value;
    memcpy(req->buffer, p->buffer(), req->size);
    req->type = REQ_DONE;
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
  const auto& type = p_ctx->header().type;
  handle[type](p_ctx);
}

inline void mv_serve_send(packet* p_ctx) {
  // Nothing to process, return.
  // packet* p_ctx = (packet*)wc.wr_id;
  if (!p_ctx) return;
  const auto& type = p_ctx->header().type;

  if (type == SEND_READY_FIN) {
    MPIV_Request* req = (MPIV_Request*)p_ctx->rdz_sreq();
    MPIV.server.write_send(req->rank, p_ctx, RNDZ_MSG_SIZE, 0);
    mv_key key = mv_make_key(req->rank, (1 << 30) | req->tag);
    mv_value value;
    if (!hash_insert(MPIV.tbl, key, value)) {
      req->type = REQ_DONE;
      thread_signal(req->sync);
    }
    stopt(rdma_timing);
    // this packet is taken directly from recv queue.
    mv_pp_free(MPIV.pkpool, p_ctx);
  } else {
    mv_pp_free(MPIV.pkpool, p_ctx, p_ctx->header().poolid);
  }
}

#endif
