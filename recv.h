#ifndef RECV_H_
#define RECV_H_

#ifndef USING_ABT

void mpiv_send_recv_ready(mpiv_packet* p, MPIV_Request* req) {
    // Makes sense to do it here, we are overlapping with other recv.
    req->dm = MPIV.dev_ctx->attach_memory(req->buffer, req->size,
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ);

    // Need to write them back, setup as a RECV_READY.
    p->rdz.idx = req->tag; // make a pair of tag and rank perhaps ?
    p->rdz.tgt_addr = (uintptr_t) req->buffer;
    p->rdz.rkey = req->dm.rkey();

    p->header.from = MPIV.me;
    p->header.type = RECV_READY;

    MPIV.conn[req->rank].write_send((void*) p,
            sizeof(mpiv_packet_header) + sizeof(p->rdz),
            MPIV.sbuf.lkey(),
            p);
}

inline void MPIV_Recv2(void* buffer, int size, int rank, int tag) {
    void* local_buf;
    MPIV_Request s(buffer, size, rank, tag);
    // Note that we do not attach the buffer here, since it will block progress
    // of other recv, only when recving a send_recv we are ready to do any hard
    // work because we know that we are going to make progress.

    // Find if the message has arrived, if not go and make a request.
    if (!MPIV.tbl.find(tag, local_buf)) {
        if (MPIV.tbl.insert(tag, (void*) &s)) {
            s.wait();
            return;
        } else {
            local_buf = MPIV.tbl[tag];
        }
    }

    // If this is inline, we handle without the buffer.
    mpiv_packet* p_ctx = (mpiv_packet*) local_buf;
    if ((size_t) size <= INLINE) {
        // This is a INLINE, copy buffer.
        memcpy(buffer, p_ctx->egr.buffer, size);
        MPIV.tbl.erase(tag);
        assert(MPIV.squeue.push(p_ctx));
        return;
    } else if ((size_t) size <= SHORT) {
        // This is a SHORT.
        // Parse the buffer.
        mpiv_packet* packet = (mpiv_packet*) p_ctx->egr.buffer;
        // Message has arrived, go and copy the message.
        memcpy(buffer, packet->egr.buffer, size);
        MPIV.tbl.erase(tag);
        assert(MPIV.squeue.push(p_ctx));
    } else {
        // This is a rdz.
        // Send RECV_READY.
        mpiv_send_recv_ready(p_ctx, &s);
        // Then update the table, so that once this is done, we know who to wake.
        MPIV.tbl[tag] = (void*) &s;
        s.wait();
    }
}
#endif

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
