#ifndef PROGRESS_H_
#define PROGRESS_H_

void mpiv_recv_recv_ready(mpiv_packet* p) {
    MPIV_Request* s = (MPIV_Request*) p->rdz.sreq;
    MPIV.conn[s->rank].write_rdma(s->buffer, MPIV.heap.lkey(),
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

void mpiv_recv_inline(mpiv_packet* p_ctx, const ibv_wc& wc) {
    startt(misc_timing);
    const uint32_t recv_size = wc.byte_len;
    const int tag = wc.imm_data;
    const int rank = MPIV.dev_ctx->get_rank(wc.qp_num);
    const mpiv_key key = mpiv_make_key(rank, tag);
    mpiv_value value;
    value.packet = p_ctx;
    stopt(misc_timing);

    startt(tbl_timing)
    mpiv_value in_val = mpiv_tbl_insert(key, value);
    stopt(tbl_timing)

    if (in_val.v == value.v) {
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
    mpiv_post_recv(p_ctx);
}

void mpiv_recv_short(mpiv_packet* p) {
    startt(misc_timing);
    const int rank = p->header.from;
    const int tag = p->header.tag;
    const mpiv_key key = mpiv_make_key(rank, tag);
    mpiv_value value;
    value.packet = p;
    stopt(misc_timing);

    startt(tbl_timing)
    mpiv_value in_val = mpiv_tbl_insert(key, value);
    stopt(tbl_timing)

    if (value.v == in_val.v) {
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
    mpiv_post_recv(p);
}

void mpiv_recv_send_ready(mpiv_packet* p) {
    mpiv_value remote_val;
    remote_val.request = (MPIV_Request*) p->rdz.sreq;

    mpiv_key key = mpiv_make_key(p->header.from, p->header.tag);
    mpiv_post_recv(p);

    startt(tbl_timing)
    mpiv_value value = mpiv_tbl_insert(key, remote_val);
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

typedef void(*p_ctx_handler)(mpiv_packet* p_ctx);
static p_ctx_handler handle[4];

static void mpiv_progress_init() {
    handle[SEND_SHORT] = mpiv_recv_short;
    handle[SEND_READY] = mpiv_recv_send_ready;
    handle[RECV_READY] = mpiv_recv_recv_ready;
    handle[SEND_READY_FIN] = mpiv_recv_send_ready_fin;
}

inline void mpiv_serve_recv(const ibv_wc& wc) {
    mpiv_packet* p_ctx = (mpiv_packet*) wc.wr_id;
    if (wc.imm_data != (uint32_t)-1) {
        // This is INLINE, we do not have header to check in some cases.
        mpiv_recv_inline(p_ctx, wc);
        return;
    } else {
        const auto& type = p_ctx->header.type;
        handle[type](p_ctx);
    }
}

inline void mpiv_serve_send(const ibv_wc& wc) {
    // Nothing to process, return.
    mpiv_packet* p_ctx = (mpiv_packet*) wc.wr_id;
    if (!p_ctx) return;
    const auto& type = p_ctx->header.type;
    if (type == SEND_READY_FIN) {
        mpiv_done_rdz_send(p_ctx);
    } else {
        MPIV.squeue.push(p_ctx);
    }
}

inline bool MPIV_Progress() {
    double t = 0;
    startt(t);
#if 0
    PAPI_read(eventSetP, t0_valueP);
#endif
    bool ret = (MPIV.dev_rcq.poll_once([](const ibv_wc& wc) {
          mpiv_serve_recv(wc);
    }));
    ret |= (MPIV.dev_scq.poll_once([](const ibv_wc& wc) {
          mpiv_serve_send(wc);
    }));
    stopt(t)
#if 0
    PAPI_read(eventSetP, t1_valueP);
    if (ret) {
        poll_timing += t;
        for (int j = 0; j<3; j++) {
            t_valueP[j] += (t1_valueP[j] - t0_valueP[j]);
        }
    }
#endif
    return ret;
}

#if 0
inline void MPI_Progress() {
    MPI_Status stat;
    int flag = 0;
    MPI_Iprobe(1, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &stat);

    // nothing.
    if (!flag) return;

    int tag = stat.MPI_TAG;
    mpiv_key key = mpiv_make_key(1, tag);

    void *sync = NULL;
    if (!mpiv_tbl_find(key, sync)) {
        // unmatched recv, allocate a buffer for it and continue.
        int size = 0;
        MPI_Get_count(&stat, MPI_BYTE, &size);
        void* buf = std::malloc(size);
        MPI_Recv(buf, size, MPI_BYTE, stat.MPI_SOURCE, tag,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        if (!mpiv_tbl_insert(key, buf)) {
            mpiv_tbl_find(key, buf);
            MPIV_Request* req = (MPIV_Request*) buf;
            memcpy(req->buffer, buf, size);
            free(buf);
            mpiv_tbl_erase(key);
            req->signal();
        } else {
            // this branch is handle
            // by the thread.
        }
    } else {
        MPIV_Request* req = (MPIV_Request*) sync;
        MPI_Recv(req->buffer, req->size, MPI_BYTE, 1, tag,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        mpiv_tbl_erase(key);
        req->signal();
    }
}
#endif

#endif
