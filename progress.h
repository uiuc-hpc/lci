#ifndef PROGRESS_H_
#define PROGRESS_H_

#ifndef USING_ABT
void mpiv_recv_recv_ready(mpiv_packet* p) {
    MPIV_Request* s = (MPIV_Request*) p->rdz.idx;
    p->header = {SEND_READY_FIN, MPIV.me, s->tag};
    MPIV.conn[s->rank].write_rdma(s->buffer, MPIV.heap.lkey(),
        (void*) p->rdz.tgt_addr, p->rdz.rkey, s->size, 0);
    MPIV.conn[s->rank].write_send(p, sizeof(p->header), 0, (void*) p);
    mpiv_post_recv(p);
}

void mpiv_recv_recv_ready_finalize(mpiv_packet* p_ctx) {
    // Now data is already ready in the buffer.
    mpiv_key key = mpiv_make_key(p_ctx->header.from, p_ctx->header.tag);
    mpiv_value value;
    mpiv_tbl_find(key, value);
    MPIV_Request* fsync = value.request;
    mpiv_tbl_erase(key);
    fsync->signal();
    mpiv_post_recv(p_ctx);
}

void mpiv_recv_inline(mpiv_packet* p_ctx, const ibv_wc& wc) {
    uint32_t recv_size = wc.byte_len;
    int tag = wc.imm_data;
    int rank = MPIV.dev_ctx->get_rank(wc.qp_num);
    mpiv_key key = mpiv_make_key(rank, tag);
    mpiv_value value;
    value.packet = p_ctx;
    if (!mpiv_tbl_find(key, value)) {
        if (mpiv_tbl_insert(key, value)) {
            // This will be handle by the thread,
            // so we return right away.
            return;
        }
        mpiv_tbl_find(key, value);
    }

    MPIV_Request* fsync = value.request;
    memcpy(fsync->buffer, (void*) p_ctx, recv_size);
    mpiv_tbl_erase(key);
    fsync->signal();
    mpiv_post_recv(p_ctx);
}

void mpiv_recv_short(mpiv_packet* p) {
    int rank = p->header.from;
    int tag = p->header.tag;
    mpiv_key key = mpiv_make_key(rank, tag);
    mpiv_value value;
    value.packet = p;

    if (!mpiv_tbl_find(key, value)) {
        if (mpiv_tbl_insert(key, value)) {
            // This will be handle by the thread,
            // so we return right away.
            return;
        }
        mpiv_tbl_find(key, value);
    }

    MPIV_Request* fsync = value.request;
    std::memcpy(fsync->buffer, p->egr.buffer, fsync->size);
    mpiv_tbl_erase(key);
    fsync->signal();
    mpiv_post_recv(p);
}

void mpiv_recv_send_ready(mpiv_packet* p) {
    mpiv_value remote_val;
    remote_val.request = (MPIV_Request*) p->rdz.idx;

    mpiv_key key = mpiv_make_key(p->header.from, p->header.tag);
    mpiv_post_recv(p);

    mpiv_value value;
    if (!mpiv_tbl_find(key, value)) {
        if (mpiv_tbl_insert(key, remote_val)) {
            // This will be handle by the thread,
            // so we return right away.
            return;
        }
        mpiv_tbl_find(key, value);
    }
    MPIV_Request* fsync = value.request;
    mpiv_send_recv_ready(remote_val.request, fsync);
}

inline void mpiv_done_rdz_send(mpiv_packet* p) {
    MPIV_Request* s = (MPIV_Request*) p->rdz.idx;
    s->signal();
}

typedef void(*p_ctx_handler)(mpiv_packet* p_ctx);
static p_ctx_handler handle[4];

static void mpiv_progress_init() {
  handle[SEND_SHORT] = mpiv_recv_short;
  handle[SEND_READY] = mpiv_recv_send_ready;
  handle[RECV_READY] = mpiv_recv_recv_ready;
  handle[SEND_READY_FIN] = mpiv_recv_recv_ready_finalize;
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
    }
    MPIV.squeue.push(p_ctx);
}

inline bool MPIV_Progress() {
    // Poll recv queue.
    return (MPIV.dev_rcq.poll_once([](const ibv_wc& wc) {
        mpiv_serve_recv(wc);
    })) || (MPIV.dev_scq.poll_once([](const ibv_wc& wc) {
        mpiv_serve_send(wc);
    }));
}
#endif

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
            MPIV_Request* fsync = (MPIV_Request*) buf;
            memcpy(fsync->buffer, buf, size);
            free(buf);
            mpiv_tbl_erase(key);
            fsync->signal();
        } else {
            // this branch is handle
            // by the thread.
        }
    } else {
        MPIV_Request* fsync = (MPIV_Request*) sync;
        MPI_Recv(fsync->buffer, fsync->size, MPI_BYTE, 1, tag,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        mpiv_tbl_erase(key);
        fsync->signal();
    }
}
#endif

#endif
