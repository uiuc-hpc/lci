#ifndef RECV_H_
#define RECV_H_

#ifndef USING_ABT


void MPIV_Recv2(void* buffer, int size, int rank, int tag) {
    void* local_buf;
    MPIV_Request s(buffer, size, rank, tag);
    mpiv_key key = mpiv_make_key(rank, tag);

    // Find if the message has arrived, if not go and make a request.
    if (!mpiv_tbl_find(key, local_buf)) {
        if (mpiv_tbl_insert(key, (void*) &s)) {
            s.wait();
            return;
        }
        mpiv_tbl_find(key, local_buf);
    }

    // If this is inline, we handle without the buffer.
    mpiv_packet* p_ctx = (mpiv_packet*) local_buf;
    if ((size_t) size <= INLINE) {
        // This is a INLINE, copy buffer.
        std::memcpy(buffer, (void*) p_ctx, size);
        mpiv_tbl_erase(key);
        mpiv_post_recv(p_ctx);
        return;
    } else if ((size_t) size <= SHORT) {
        // This is a SHORT.
        // Parse the buffer.
        mpiv_packet* packet = (mpiv_packet*) p_ctx;
        // Message has arrived, go and copy the message.
        std::memcpy(buffer, packet->egr.buffer, size);
        mpiv_tbl_erase(key);
        mpiv_post_recv(p_ctx);
    } else {
        // This is a rdz.
        // Send RECV_READY.
        mpiv_tbl_update(key, (void*) &s);
        mpiv_send_recv_ready((uintptr_t) local_buf, &s);
        // Then update the table, so that once this is done, we know who to wake.
        s.wait();
    }
}
#endif

void MPIV_Recv(void* buffer, int size, int rank, int tag) {
    void* local_buf;
    mpiv_key key = mpiv_make_key(rank, tag);
    if (!mpiv_tbl_find(key, local_buf)) {
        MPIV_Request s(buffer, size, rank, tag);
        if (mpiv_tbl_insert(key, (void*) &s)) {
            s.wait();
            return;
        } else {
            mpiv_tbl_find(key, local_buf);
        }
    }
    std::memcpy(buffer, local_buf, size);
    free(local_buf);
    mpiv_tbl_erase(key);
}

#endif
