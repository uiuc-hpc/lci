#ifndef RECV_H_
#define RECV_H_

void MPIV_Recv(void* buffer, int size, int rank, int tag) {
    mpiv_value value;
    MPIV_Request s(buffer, size, rank, tag);

    mpiv_key key = mpiv_make_key(rank, tag);
    value.request = &s;

    // Find if the message has arrived, if not go and make a request.
    if (!mpiv_tbl_find(key, value)) {
        if (mpiv_tbl_insert(key, value)) {
            s.wait();
            mpiv_tbl_erase(key);
            return;
        }
        mpiv_tbl_find(key, value);
    }

    // If this is inline, we handle without the buffer.
    mpiv_packet* p_ctx = value.packet;
    if ((size_t) size <= INLINE) {
        // This is a INLINE, copy buffer.
        std::memcpy(buffer, (void*) p_ctx, size);
        mpiv_tbl_erase(key);
        mpiv_post_recv(p_ctx);
        return;
    } else if ((size_t) size <= SHORT) {
        // This is a SHORT.
        // Parse the buffer.
        // Message has arrived, go and copy the message.
        std::memcpy(buffer, p_ctx->egr.buffer, size);
        mpiv_tbl_erase(key);
        mpiv_post_recv(p_ctx);
    } else {
        // This is a rdz.
        // Send RECV_READY.
        MPIV_Request* remote_req = value.request;
        mpiv_send_recv_ready(remote_req, &s);
        s.wait();
        mpiv_tbl_erase(key);
    }
}

#endif
