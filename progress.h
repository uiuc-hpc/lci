#ifndef PROGRESS_H_
#define PROGRESS_H_

#ifndef USING_ABT
inline void mpiv_recv_recv_ready(mpiv_packet* p_ctx, mpiv_packet* p) {
    // printf("sending: %p %d t: %p %d sz: %d\n", p->rdz.src_addr, p->rdz.lkey, p->rdz.tgt_addr,
    //    p->rdz.rkey, p->rdz.size);
    int tag = p->rdz.idx;
    void* buf;
    // printf("GETTING... %p %d\n", (void*) p->rdz.tgt_addr, p->rdz.rkey);

    if (!MPIV.rdztbl.find(tag, buf)) {
        if (MPIV.rdztbl.insert(tag, p_ctx)) {
            // handle by thread.
            return;
        }
        buf = MPIV.rdztbl[tag];
    }

    // Else, thread win, we handle.
    MPIV_Request* s = (MPIV_Request*) buf;
    p_ctx->header.type = SEND_READY_FIN;

    MPIV.conn[p->header.from].write_rdma_signal(s->buffer, s->dm.lkey(),
        (void*) p->rdz.tgt_addr, p->rdz.rkey, p->rdz.size, p_ctx,
        p->rdz.idx);
}

inline void mpiv_recv_recv_ready_finalize(int tag) {
    // Now data is already ready in the buffer.
    void *sync = NULL;
    MPIV_Request* fsync = NULL;
    MPIV.tbl.find(tag, sync);
    fsync = (MPIV_Request*) sync;
    fsync->dm.finalize();
    MPIV.tbl.erase(tag);
    fsync->signal();
}

inline void mpiv_recv_inline(mpiv_packet* p_ctx, const ibv_wc& wc) {
    uint32_t recv_size = wc.byte_len;
    int tag = wc.imm_data;
    void *sync = NULL;
    MPIV_Request* fsync = NULL;
    if (!MPIV.tbl.find(tag, sync)) {
        if (MPIV.tbl.insert(tag, (void*) p_ctx)) {
            // This will be handle by the thread,
            // so we return right away.
            return;
        }
        fsync = (MPIV_Request*) (void*) MPIV.tbl[tag];
    } else {
        fsync = (MPIV_Request*) sync;
    }
    memcpy(fsync->buffer, p_ctx->egr.buffer, recv_size);
    MPIV.tbl.erase(tag);
    assert(MPIV.squeue.push(p_ctx));
    fsync->signal();
}

inline void mpiv_recv_short(mpiv_packet* p_ctx,
        mpiv_packet* p, const ibv_wc& wc) {
    uint32_t recv_size = wc.byte_len - sizeof(mpiv_packet_header);
    int tag = p->header.tag;
    // printf("RECV size %d tag %d ctx %p pointer %p\n", recv_size, tag, p_ctx, p);

    void *sync = NULL;
    MPIV_Request* fsync = NULL;
    if (!MPIV.tbl.find(tag, sync)) {
        if (MPIV.tbl.insert(tag, (void*) p_ctx)) {
            // This will be handle by the thread,
            // so we return right away.
            return;
        }
        fsync = (MPIV_Request*) (void*) MPIV.tbl[tag];
    } else {
        fsync = (MPIV_Request*) sync;
    }

    memcpy(fsync->buffer, p->egr.buffer, recv_size);
    MPIV.tbl.erase(tag);
    assert(MPIV.squeue.push(p_ctx));
    fsync->signal();
}

inline void mpiv_recv_send_ready(mpiv_packet* p_ctx, mpiv_packet* p) {
    int tag = p->header.tag;
    void *sync = NULL;
    MPIV_Request* fsync = NULL;
    if (!MPIV.tbl.find(tag, sync)) {
        if (MPIV.tbl.insert(tag, (void*) p_ctx)) {
            // This will be handle by the thread,
            // so we return right away.
            return;
        }
        fsync = (MPIV_Request*) (void*) MPIV.tbl[tag];
    } else {
        fsync = (MPIV_Request*) sync;
    }
    mpiv_send_recv_ready(p_ctx, fsync);
}

inline void done_rdz_send(mpiv_packet* packet) {
    mpiv_packet* p = (mpiv_packet*) packet->egr.buffer;
    int tag = p->rdz.idx;
    void* buf = MPIV.rdztbl[tag];
    MPIV_Request* s = (MPIV_Request*) buf;
    MPIV.rdztbl.erase(tag);
    s->dm.finalize();
    s->signal();
}

inline void MPIV_Progress() {
    // Poll recv queue.
    MPIV.dev_rcq.poll_once([](const ibv_wc& wc) {
        mpiv_packet* p_ctx = (mpiv_packet*) wc.wr_id;
        if (wc.opcode == IBV_WC_RECV_RDMA_WITH_IMM) {
            // FIXME: how do I get the rank ? From qpn probably ?
            // And how do I know this is the RECV_READY ?
            int tag = (int) wc.imm_data & 0xffffffff;
            mpiv_recv_recv_ready_finalize(tag);
            // return the packet.
            assert(MPIV.squeue.push(p_ctx));
        } else if (wc.imm_data != (uint32_t)-1) {
            // This is INLINE.
            mpiv_recv_inline(p_ctx, wc);
        } else {
            // Other cases.
            mpiv_packet* p = (mpiv_packet*) p_ctx->egr.buffer;
            if (p->header.type == SEND_SHORT) {
                // Return true if we need to return this p_ctx.
                mpiv_recv_short(p_ctx, p, wc);
            } else if (p->header.type == SEND_READY) {
                mpiv_recv_send_ready(p_ctx, p);
            } else if (p->header.type == RECV_READY) {
                mpiv_recv_recv_ready(p_ctx, p);
            } else {
                assert(0 && "Invalid packet or not implemented");
            }
        }
        // Post back a recv, as we have just consumed one.
        mpiv_post_recv(get_freesbuf());
    });

    // Poll send queue.
    MPIV.dev_scq.poll_once([](const ibv_wc& wc) {
        if (wc.wr_id == 0) {
            // This is an INLINE, do nothing.
            return;
        }
        mpiv_packet* packet = (mpiv_packet*) wc.wr_id;
        if (packet->header.type == SEND_SHORT) {
            // done.
        } else if (packet->header.type == SEND_READY) {
            // expecting RECV_READY next.
        } else if (packet->header.type == RECV_READY) {
            // expecting RDMA next.
        } else if (packet->header.type == SEND_READY_FIN) {
            done_rdz_send(packet);
            // done rdz send.
        } else {
            assert(0 && "Invalid packet or not implemented");
        }

        // Return the packet, as we have just used one.
        assert(MPIV.squeue.push(packet));
    });
}
#endif

inline void MPI_Progress() {
    MPI_Status stat;
    int flag = 0;
    MPI_Iprobe(1, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &stat);

    // nothing.
    if (!flag) return;

    int tag = stat.MPI_TAG;
    void *sync = NULL;
    if (!MPIV.tbl.find(tag, sync)) {
        // unmatched recv, allocate a buffer for it and continue.
        int size = 0;
        MPI_Get_count(&stat, MPI_BYTE, &size);
        void* buf = std::malloc(size);
        MPI_Recv(buf, size, MPI_BYTE, stat.MPI_SOURCE, tag,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        if (!MPIV.tbl.insert(tag, buf)) {
            MPIV_Request* fsync = (MPIV_Request*) (void*) MPIV.tbl[tag];
            memcpy(fsync->buffer, buf, size);
            fsync->signal();
            free(buf);
            MPIV.tbl.erase(tag);
        } else {
            // this branch is handle
            // by the thread.
        }
    } else {
        MPIV_Request* fsync = (MPIV_Request*) sync;
        MPI_Recv(fsync->buffer, fsync->size, MPI_BYTE, 1, tag,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        fsync->signal();
        MPIV.tbl.erase(tag);
    }
}

#endif
