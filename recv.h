#ifndef RECV_H_
#define RECV_H_

#ifndef USING_ABT

void mpiv_send_recv_ready(mpiv_packet* p, MPIV_Request* req) {
    // Need to write them back, setup as a RECV_READY.
    p->header = {RECV_READY, MPIV.me, req->tag};
    p->rdz = {(uintptr_t) req->buffer, MPIV.heap.rkey(), (uint32_t) req->size};

    MPIV.conn[req->rank].write_send((void*) p,
            sizeof(mpiv_packet_header) + sizeof(p->rdz),
            MPIV.sbuf.lkey(), 0);
    assert(MPIV.squeue.push(p));
}

inline void MPIV_Recv2(void* buffer, int size, int rank, int tag) {
    MPIV.pending++;
    void* local_buf;
    MPIV_Request s(buffer, size, rank, tag);
    mpiv_key key = mpiv_make_key(rank, tag);

    // Find if the message has arrived, if not go and make a request.
    if (!MPIV.tbl.find(key, local_buf)) {
        if (MPIV.tbl.insert(key, (void*) &s)) {
            s.wait();
            MPIV.pending--;
            return;
        } else {
            local_buf = MPIV.tbl[key];
        }
    }

    // If this is inline, we handle without the buffer.
    mpiv_packet* p_ctx = (mpiv_packet*) local_buf;
    if ((size_t) size <= INLINE) {
        // This is a INLINE, copy buffer.
        std::memcpy(buffer, (void*) p_ctx, size);
        MPIV.tbl.erase(key);
        mpiv_post_recv(p_ctx);
        MPIV.pending--;
        return;
    } else if ((size_t) size <= SHORT) {
        // This is a SHORT.
        // Parse the buffer.
        mpiv_packet* packet = (mpiv_packet*) p_ctx;
        // Message has arrived, go and copy the message.
        std::memcpy(buffer, packet->egr.buffer, size);
        MPIV.tbl.erase(key);
        assert(MPIV.squeue.push(p_ctx));
    } else {
        // This is a rdz.
        // Send RECV_READY.
        MPIV.tbl.update(key, (void*) &s);
        mpiv_send_recv_ready(p_ctx, &s);
        // Then update the table, so that once this is done, we know who to wake.
        s.wait();
    }
    MPIV.pending--;
}
#endif

void MPIV_Recv(void* buffer, int size, int rank, int tag) {
    void* local_buf;
    mpiv_key key = mpiv_make_key(rank, tag);
    if (!MPIV.tbl.find(key, local_buf)) {
        MPIV_Request s(buffer, size, rank, tag);
        if (MPIV.tbl.insert(key, (void*) &s)) {
            s.wait();
            return;
        } else {
            local_buf = MPIV.tbl[key];
        }
    }
    std::memcpy(buffer, local_buf, size);
    free(local_buf);
    MPIV.tbl.erase(key);
}

#endif
