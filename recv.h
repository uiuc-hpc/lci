#ifndef RECV_H_
#define RECV_H_

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
    fsync->signal();
    MPIV.tbl.erase(tag);
    mpiv_post_recv(p_ctx);
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
    fsync->signal();
    MPIV.tbl.erase(tag);
    mpiv_post_recv(p_ctx);
}

void mpiv_rdz_send(mpiv_packet* p_ctx, mpiv_packet* p, void* pinned_buffer) {
    // Attach the buffer (if not already).
    device_memory* dm;
    if (!pinned.find(pinned_buffer, dm)) {
        dm = new device_memory{MPIV.dev_ctx->attach_memory(pinned_buffer, p->rdz.size,
            IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ)};
        pinned.insert(pinned_buffer, dm);
        // printf("attached: %d\n", dm.rkey());
    }

    int from = p->header.from;

    // Need to write them back, setup as a RECV_READY.
    p->rdz.idx = p->header.tag; // make a pair of tag and rank perhaps ?
    p->rdz.tgt_addr = (uintptr_t) pinned_buffer;
    p->rdz.rkey = dm->rkey();
    p->header.from = MPIV.me;
    p->header.type = RECV_READY;

    p_ctx->header.type = RECV_READY;

    MPIV.conn[from].write_send((void*) p,
            sizeof(mpiv_packet_header) + sizeof(p->rdz),
            MPIV.rbuf.lkey(),
            p_ctx);
    // printf("Done send %p\n", p_ctx);
}

inline void mpiv_recv_send_ready(mpiv_packet* p_ctx, mpiv_packet* p) {
    int tag = p->header.tag;
    // printf("%p recv send_ready \n", p_ctx);

    void *sync = NULL;
    MPIV_Request* fsync = NULL;
    if (!MPIV.tbl.find(tag, sync)) {
        if (MPIV.tbl.insert(tag, (void*) p_ctx)) {
            // printf("%p added \n", p_ctx);
            // This will be handle by the thread,
            // so we return right away.
            return;
        }
        fsync = (MPIV_Request*) (void*) MPIV.tbl[tag];
    } else {
        fsync = (MPIV_Request*) sync;
    }

    // printf("got buffer %p -> %p\n", fsync, fsync->buffer);
    mpiv_rdz_send(p_ctx, p, fsync->buffer);
}

inline void mpiv_recv_recv_ready(mpiv_packet* p_ctx, mpiv_packet* p) {
    // printf("sending: %p %d t: %p %d sz: %d\n", p->rdz.src_addr, p->rdz.lkey, p->rdz.tgt_addr,
    //    p->rdz.rkey, p->rdz.size);

    p_ctx->header.type = SEND_READY_FIN;
    MPIV.conn[p->header.from].write_rdma_signal((void*) p->rdz.src_addr, p->rdz.lkey,
        (void*) p->rdz.tgt_addr, p->rdz.rkey, p->rdz.size, p_ctx,
        p->rdz.idx);
}

inline void mpiv_recv_recv_ready_finalize(mpiv_packet* p_ctx, int tag) {
    // Now data is already ready in the buffer.
    void *sync = NULL;
    MPIV_Request* fsync = NULL;

    MPIV.tbl.find(tag, sync);
    //if ((uintptr_t) fsync->buffer != p->rdz.tgt_addr)
    // memcpy(fsync->buffer, (void*) p->rdz.tgt_addr, p->rdz.size);

    fsync = (MPIV_Request*) sync;
    fsync->signal();
    MPIV.tbl.erase(tag);
    mpiv_post_recv(p_ctx);
}

inline void MPIV_Recv2(void* buffer, int size, int rank, int tag) {
    void* local_buf;
    MPIV_Request s(buffer, size, rank, tag);

    // Find if the message has arrived, if not go and make a request.
    if (!MPIV.tbl.find(tag, local_buf)) {
        if (MPIV.tbl.insert(tag, (void*) &s)) {
            s.wait();
            return;
        } else {
            local_buf = MPIV.tbl[tag];
        }
    }
    mpiv_packet* p_ctx = (mpiv_packet*) local_buf;
    if ((uint32_t) size <= INLINE) {
        memcpy(buffer, p_ctx->egr.buffer, size);
        MPIV.tbl.erase(tag);
        mpiv_post_recv(p_ctx);
        return;
    }

    // Now we need packet.
    mpiv_packet* packet = (mpiv_packet*) p_ctx->egr.buffer;
    if ((uint32_t) size <= SHORT) {
        // Message has arrived, go and copy the message.
        memcpy(buffer, packet->egr.buffer, size);
        MPIV.tbl.erase(tag);
        mpiv_post_recv(p_ctx);
    } else {
        // Buffer has arrived, go and send it back.
        // printf("%p arrived, sending %p\n", buffer, p_ctx);
        mpiv_rdz_send(p_ctx, packet, buffer);
        MPIV.tbl.update(tag, (void*) &s);
        s.wait();
        // Now wait for the final message.
    }
}

void MPIV_Recv(void* buffer, int size, int rank, int tag) {
    void* local_buf;
    if (!MPIV.tbl.find(tag, local_buf)) {
        MPIV_Request s(buffer, size, rank, tag);
        if (MPIV.tbl.insert(tag, (void*) &s)) {
            s.wait();
            return;
        } else {
            local_buf = MPIV.tbl[tag];
        }
    }
    memcpy(buffer, local_buf, size);
    free(local_buf);
    MPIV.tbl.erase(tag);
}

#endif
