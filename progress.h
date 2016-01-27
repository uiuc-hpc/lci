#ifndef PROGRESS_H_
#define PROGRESS_H_

#include "mpiv.h"

void mpiv_post_recv(mpiv_packet*);

void mpiv_recv_recv_ready(mpiv_packet* p) {
  MPIV_Request* s = (MPIV_Request*)p->rdz.sreq;
  MPIV.ctx.conn[s->rank].write_rdma(s->buffer, MPIV.ctx.heap_lkey,
                                    (void*)p->rdz.tgt_addr, p->rdz.rkey,
                                    s->size, 0);

  p->header = {SEND_READY_FIN, MPIV.me, s->tag};
  MPIV.ctx.conn[s->rank].write_send(p, 64, 0, (void*)p);
  mpiv_post_recv(p);
}

void mpiv_recv_send_ready_fin(mpiv_packet* p_ctx) {
  // Now data is already ready in the buffer.
  MPIV_Request* req = (MPIV_Request*)p_ctx->rdz.rreq;
  startt(signal_timing) startt(wake_timing) req->sync.signal();
  stopt(signal_timing) mpiv_post_recv(p_ctx);
}

#if 0
void mpiv_recv_inline(mpiv_packet* p_ctx, const ibv_wc& wc) {
    startt(misc_timing);
    const uint32_t recv_size = wc.byte_len;
    const int tag = wc.imm_data + 1;
    const int rank = MPIV.server->get_rank(wc.qp_num);
    const mpiv_key key = mpiv_make_key(rank, tag);
    mpiv_value value;
    value.packet = p_ctx;
    stopt(misc_timing);

    startt(tbl_timing)
    auto in_val = MPIV.tbl.insert(key, value).first;
    stopt(tbl_timing)

    if (in_val.v == value.v) {
        startt(post_timing)
        mpiv_post_recv(MPIV.pk_mgr.get_packet());
        stopt(post_timing)
        // This will be handle by the thread,
        // so we return right away.
        return;
    }
    MPIV_Request* req = in_val.request;
    startt(memcpy_timing)
    memcpy(req->buffer, (void*) p_ctx, recv_size);
    stopt(memcpy_timing)

    startt(signal_timing)
    startt(wake_timing);
    req->sync.signal();
    stopt(signal_timing)

    startt(post_timing)
    mpiv_post_recv(p_ctx);
    stopt(post_timing)
}
#endif

std::atomic<int> prepost;

void mpiv_recv_short(mpiv_packet* p) {
  startt(misc_timing);
  const int rank = p->header.from;
  const int tag = p->header.tag;
  const mpiv_key key = mpiv_make_key(rank, tag);
  mpiv_value value;
  value.packet = p;
  stopt(misc_timing);

  startt(tbl_timing) auto in_val = MPIV.tbl.insert(key, value).first;
  stopt(tbl_timing)

      if (value.v == in_val.v) {
    startt(post_timing) mpiv_post_recv(MPIV.pk_mgr.get_packet());
    stopt(post_timing)
        // This will be handle by the thread,
        // so we return right away.
        return;
  }
  MPIV_Request* req = in_val.request;

  startt(memcpy_timing) memcpy(req->buffer, p->egr.buffer, req->size);
  stopt(memcpy_timing)

      startt(signal_timing) startt(wake_timing) req->sync.signal();
  stopt(signal_timing)

      startt(post_timing) mpiv_post_recv(p);
  stopt(post_timing)
}

void mpiv_recv_send_ready(mpiv_packet* p) {
  mpiv_value remote_val;
  remote_val.request = (MPIV_Request*)p->rdz.sreq;

  mpiv_key key = mpiv_make_key(p->header.from, p->header.tag);
  mpiv_post_recv(p);

  startt(tbl_timing) auto value = MPIV.tbl.insert(key, remote_val).first;
  stopt(tbl_timing)

      if (value.v == remote_val.v) {
    // This will be handle by the thread,
    // so we return right away.
    return;
  }
  MPIV_Request* req = value.request;
  mpiv_send_recv_ready(remote_val.request, req);
}

typedef void (*p_ctx_handler)(mpiv_packet* p_ctx);
static p_ctx_handler handle[4];

static void mpiv_progress_init() {
  handle[SEND_SHORT] = mpiv_recv_short;
  handle[SEND_READY] = mpiv_recv_send_ready;
  handle[RECV_READY] = mpiv_recv_recv_ready;
  handle[SEND_READY_FIN] = mpiv_recv_send_ready_fin;
}

inline void mpiv_serve_recv(const ibv_wc& wc) {
  mpiv_packet* p_ctx = (mpiv_packet*)wc.wr_id;
#if 0
    if (wc.byte_len <= INLINE && wc.imm_data != 0) {
        // This is INLINE, we do not have header to check in some cases.
        mpiv_recv_inline(p_ctx, wc);
        return;
    } else
#endif
  {
    const auto& type = p_ctx->header.type;
    handle[type](p_ctx);
  }
}

inline void mpiv_serve_send(const ibv_wc& wc) {
  // Nothing to process, return.
  mpiv_packet* p_ctx = (mpiv_packet*)wc.wr_id;
  if (!p_ctx) return;
  const auto& type = p_ctx->header.type;
  if (type == SEND_READY_FIN) {
    MPIV_Request* req = (MPIV_Request*)p_ctx->rdz.sreq;
    req->sync.signal();
    stopt(rdma_timing);
  } else {
    MPIV.pk_mgr.new_packet(p_ctx);
  }
}

#endif
