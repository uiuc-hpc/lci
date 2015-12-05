#ifndef RECV_H_
#define RECV_H_

void MPIV_Send(void* buffer, size_t size, int rank, int tag);

void MPIV_Recv(void* buffer, size_t size, int rank, int tag) {
    mpiv_value value;
    MPIV_Request s(buffer, size, rank, tag);

    mpiv_key key = mpiv_make_key(rank, tag);
    value.request = &s;

    if ((size_t) size > SHORT) {
        // RDNZ protocol, use SEND + WAIT.
        startt(misc_timing);
        char data[64];
        mpiv_packet* p = (mpiv_packet*) data;
        p->header = {RECV_READY, MPIV.me, tag};
        p->rdz = {0, (uintptr_t) &s, (uintptr_t) buffer, MPIV.heap.rkey(), (uint32_t) size};
        MPIV_Send(data, 64, rank, tag);
        MPIV_Wait(s);
        stopt(misc_timing);
        return;
    }

    // Find if the message has arrived, if not go and make a request.
    startt(tbl_timing)
    mpiv_value in_val = mpiv_tbl_insert(key, value);
    stopt(tbl_timing)

    if (value.v == in_val.v) {
        MPIV_Wait(s);
        stopt(wake_timing)
    } else {
        // If this is inline, we handle without the buffer.
        mpiv_packet* p_ctx = in_val.packet;
        if (size <= INLINE) {
            // This is a INLINE, copy buffer.
            startt(memcpy_timing);
            memcpy(buffer, (void*) p_ctx, size);
            stopt(memcpy_timing);
            startt(post_timing)
            mpiv_post_recv(p_ctx);
            stopt(post_timing)
        } else if (size <= SHORT) {
            // This is a SHORT.
            // Parse the buffer.
            // Message has arrived, go and copy the message.
            startt(memcpy_timing)
            std::memcpy(buffer, p_ctx->egr.buffer, size);
            stopt(memcpy_timing);
            startt(post_timing)
            mpiv_post_recv(p_ctx);
            stopt(post_timing)
        }
#if 0        
        else {
            // This is a rdz.
            // Send RECV_READY.
            startt(misc_timing)
            MPIV_Request* remote_req = value.request;
            mpiv_send_recv_ready(remote_req, &s);
            s.wait();
            stopt(misc_timing)
        }
#endif
    }

    startt(tbl_timing)
    mpiv_tbl_erase(key);
    stopt(tbl_timing)
}

#endif
