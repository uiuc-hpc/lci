#ifndef PROGRESS_H_
#define PROGRESS_H_

#include "mpiv.h"

void mpiv_complete_rndz(mpiv_packet* p, MPIV_Request* s) {
  p->set_header(SEND_READY_FIN, MPIV.me, s->tag);
  p->set_sreq((uintptr_t) s);
  MPIV.ctx.conn[s->rank].write_rdma(s->buffer, MPIV.ctx.heap_lkey,
      (void*)p->rdz_tgt_addr(), p->rdz_rkey(),
      s->size, 0);
  MPIV.ctx.conn[s->rank].write_send(p, RNDZ_MSG_SIZE, 0, p);
}

void mpiv_recv_recv_ready(mpiv_packet* p) {
  mpiv_key key = p->get_rdz_key();
  mpiv_value value;
  value.packet = p;
  auto entry = MPIV.tbl.insert(key, value);
  if (entry.first.v != value.v) {
    MPIV_Request* s = entry.first.request;
    mpiv_complete_rndz(p, s);
    MPIV.tbl.erase(key, entry.second);
  }
}

void mpiv_recv_send_ready_fin(mpiv_packet* p_ctx) {
  // Now data is already ready in the buffer.
  MPIV_Request* req = (MPIV_Request*)(p_ctx->rdz_rreq());
  startt(signal_timing);
  startt(wake_timing);
  MPIV_Signal(req);
  stopt(signal_timing);
  MPIV.recvpk.ret_packet(p_ctx);
}

void mpiv_recv_short(mpiv_packet* p) {
  startt(misc_timing);
  const mpiv_key key = p->get_key();
  mpiv_value value;
  value.packet = p;
  stopt(misc_timing);

  startt(tbl_timing);
  auto in_val = MPIV.tbl.insert(key, value).first;
  stopt(tbl_timing);

  if (value.v != in_val.v) {
    // comm-thread comes later.
    MPIV_Request* req = in_val.request; 
    if (req->size >= SERVER_COPY_SIZE)  {
      req->buffer = (void*) p;
      MPIV_Signal(req);
    } else {
      memcpy(req->buffer, p->buffer(), req->size);
      MPIV_Signal(req);
      MPIV.recvpk.ret_packet(p);
    }
  }
}

void mpiv_recv_send_ready(mpiv_packet* p) {
  mpiv_value remote_val;
  remote_val.request = (MPIV_Request*)p->rdz_sreq();

  mpiv_key key = p->get_key(); //mpiv_make_key(p->header.from, p->header.tag);
  MPIV.recvpk.ret_packet(p);

  startt(tbl_timing);
  auto value = MPIV.tbl.insert(key, remote_val).first;
  stopt(tbl_timing);

  if (value.v == remote_val.v) {
    // This will be handle by the thread,
    // so we return right away.
    return;
  }
  MPIV_Request* req = value.request;
  mpiv_send_recv_ready(remote_val.request, req);
}

typedef void (*p_ctx_handler)(mpiv_packet* p_ctx);
static p_ctx_handler* handle;

static void mpiv_progress_init() {
  posix_memalign((void**) &handle, 64, 64);
  handle[SEND_SHORT] = mpiv_recv_short;
  handle[SEND_READY] = mpiv_recv_send_ready;
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

  if (type == SEND_READY_FIN) {
    MPIV_Request* req = (MPIV_Request*)p_ctx->rdz_sreq();
    MPIV_Signal(req);
    stopt(rdma_timing);
    // this packet is taken directly from recv queue.
    MPIV.recvpk.ret_packet(p_ctx);
  } else {
    MPIV.sendpk.ret_packet(p_ctx);
  }
}

#endif
