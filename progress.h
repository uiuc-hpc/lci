#ifndef PROGRESS_H_
#define PROGRESS_H_

void mpiv_post_recv(mpiv_packet*);

void mpiv_recv_recv_ready(mpiv_packet* p) {
    MPIV_Request* s = (MPIV_Request*) p->rdz.sreq;
    MPIV.conn[s->rank].write_rdma(s->buffer, MPIV.heap_lkey,
        (void*) p->rdz.tgt_addr, p->rdz.rkey, s->size, 0);

    p->header = {SEND_READY_FIN, MPIV.me, s->tag};
    MPIV.conn[s->rank].write_send(p, 64, 0, (void*) p);
    mpiv_post_recv(p);
}

void mpiv_recv_send_ready_fin(mpiv_packet* p_ctx) {
    // Now data is already ready in the buffer.
    MPIV_Request* req = (MPIV_Request*) p_ctx->rdz.rreq;
    startt(signal_timing)
    startt(wake_timing)
    req->sync.signal();
    stopt(signal_timing)
    mpiv_post_recv(p_ctx);
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

    startt(tbl_timing)
    auto in_val = MPIV.tbl.insert(key, value).first;
    stopt(tbl_timing)

    if (value.v == in_val.v) {
        startt(post_timing)
        mpiv_post_recv(MPIV.pk_mgr.get_packet());
        stopt(post_timing)
        // This will be handle by the thread,
        // so we return right away.
        return;
    }
    MPIV_Request* req = in_val.request;

    startt(memcpy_timing)
    memcpy(req->buffer, p->egr.buffer, req->size);
    stopt(memcpy_timing)

    startt(signal_timing)
    startt(wake_timing)
    req->sync.signal();
    stopt(signal_timing)

    startt(post_timing)
    mpiv_post_recv(p);
    stopt(post_timing)
}

void mpiv_recv_send_ready(mpiv_packet* p) {
    mpiv_value remote_val;
    remote_val.request = (MPIV_Request*) p->rdz.sreq;

    mpiv_key key = mpiv_make_key(p->header.from, p->header.tag);
    mpiv_post_recv(p);

    startt(tbl_timing)
    auto value = MPIV.tbl.insert(key, remote_val).first;
    stopt(tbl_timing)

    if (value.v == remote_val.v) {
        // This will be handle by the thread,
        // so we return right away.
        return;
    }
    MPIV_Request* req = value.request;
    mpiv_send_recv_ready(remote_val.request, req);
}

inline void mpiv_done_rdz_send(mpiv_packet* p) {
    stopt(rdma_timing);
    MPIV_Request* req = (MPIV_Request*) p->rdz.sreq;
    req->sync.signal();
}

#endif
