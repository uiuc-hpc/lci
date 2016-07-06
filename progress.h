#ifndef PROGRESS_H_
#define PROGRESS_H_

#include "mpiv.h"

void mpiv_post_recv(mpiv_packet* p);

void mpiv_complete_rndz(mpiv_packet* p, MPIV_Request* s) {
  p->set_header(SEND_READY_FIN, MPIV.me, s->tag);
  p->set_sreq((uintptr_t)s);
  MPIV.ctx.conn[s->rank].write_rdma(s->buffer, MPIV.ctx.heap_lkey,
                                    (void*)p->rdz_tgt_addr(), p->rdz_rkey(),
                                    s->size, 0);
  MPIV.ctx.conn[s->rank].write_send(p, RNDZ_MSG_SIZE, 0, (void*)p);
}

void mpiv_recv_recv_ready(mpiv_packet* p) {
  mpiv_key key = p->get_rdz_key();
  mpiv_value value;
  value.packet = p;
  if (!MPIV.tbl.insert(key, value)) {
    mpiv_complete_rndz(p, value.request);
  }
}

void mpiv_recv_send_ready_fin(mpiv_packet* p_ctx) {
  // Now data is already ready in the buffer.
  MPIV_Request* req = (MPIV_Request*)(p_ctx->rdz_rreq());
  assert(req);
  startt(signal_timing);
  startt(wake_timing);
  MPIV_Signal(req);
  stopt(signal_timing);
  MPIV.pkpool.ret_packet(p_ctx);
}

void mpiv_post_recv(mpiv_packet*);

void mpiv_recv_short(mpiv_packet* p) {
  startt(misc_timing);
  const mpiv_key key = p->get_key();
  mpiv_value value;
  value.packet = p;
  stopt(misc_timing);

  if (!MPIV.tbl.insert(key, value)) {
    // comm-thread comes later.
    MPIV_Request* req = value.request;
    memcpy(req->buffer, p->buffer(), req->size);

    MPIV_Signal(req);
    MPIV.pkpool.ret_packet(p);
  }
}

typedef void (*p_ctx_handler)(mpiv_packet* p_ctx);
static p_ctx_handler* handle;

static void mpiv_progress_init() {
  posix_memalign((void**)&handle, 64, 64);
  handle[SEND_SHORT] = mpiv_recv_short;
  handle[RECV_READY] = mpiv_recv_recv_ready;
  handle[SEND_READY_FIN] = mpiv_recv_send_ready_fin;
}

inline void mpiv_serve_recv(const ibv_wc& wc) {
  mpiv_packet* p_ctx = (mpiv_packet*)wc.wr_id;
  const auto& type = p_ctx->header().type;
  handle[type](p_ctx);
}

inline void mpiv_serve_send(const ibv_wc& wc) {
  // Nothing to process, return.
  mpiv_packet* p_ctx = (mpiv_packet*)wc.wr_id;
  if (!p_ctx) return;
  const auto& type = p_ctx->header().type;
  auto poolid = p_ctx->poolid();

  if (type == SEND_READY_FIN) {
    MPIV_Request* req = (MPIV_Request*)p_ctx->rdz_sreq();
    MPIV_Signal(req);
    stopt(rdma_timing);
    // this packet is taken directly from recv queue.
    //if (MPIV.server.need_recv()) mpiv_post_recv(p_ctx); else
    MPIV.pkpool.ret_packet(p_ctx);
  } else {
    // if (MPIV.server.need_recv()) mpiv_post_recv(p_ctx); else
    // MPIV_Signal((MPIV_Request*) p_ctx->header().sreq);
    MPIV.pkpool.ret_packet_to(p_ctx, poolid);
  }
}

#endif
