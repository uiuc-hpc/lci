#ifndef PROGRESS_H_
#define PROGRESS_H_

inline void MPIV_Progress() {
    // Poll recv queue.
    MPIV.dev_rcq.poll_once([](const ibv_wc& wc) {
        if (wc.opcode == IBV_WC_RECV_RDMA_WITH_IMM) {
            // FIXME: how do I get the rank ? From qpn probably ?
            // And how do I know this is the RECV_READY ?
            int tag = (int) wc.imm_data & 0xffffffff;
            mpiv_packet* p_ctx = (mpiv_packet*) wc.wr_id;
            mpiv_recv_recv_ready_finalize(p_ctx, tag);
        } else if (wc.imm_data != (uint32_t)-1) {
            // This is INLINE.
            mpiv_packet* p_ctx = (mpiv_packet*) wc.wr_id;
            mpiv_recv_inline(p_ctx, wc);
        } else {
            mpiv_packet* p_ctx = (mpiv_packet*) wc.wr_id;
            mpiv_packet* p = (mpiv_packet*) &(p_ctx->egr.buffer);
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
    });

    // Poll send queue.
    MPIV.dev_scq.poll_once([](const ibv_wc& wc) {
        if (wc.wr_id == 0) {
            // This is an INLINE, do nothing.
            return;
        }
        mpiv_packet* packet = (mpiv_packet*) wc.wr_id;
        // Return this free packet to the queue.
        if (packet->header.type == SEND_SHORT) {
            assert(MPIV.squeue.push(packet));
        } else if (packet->header.type == SEND_READY) {
            assert(MPIV.squeue.push(packet));
        } else if (packet->header.type == RECV_READY) {
            // Done sending the tgt_addr, return the packet.
            mpiv_post_recv(packet);
        } else if (packet->header.type == SEND_READY_FIN) {
            // finished rdz protocol for send.
            mpiv_post_recv(packet);
        } else {
            assert(0 && "Invalid packet or not implemented");
        }
    });
}

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
