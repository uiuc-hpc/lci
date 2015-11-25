#ifndef SEND_H_
#define SEND_H_

#ifndef USING_ABT

inline void MPIV_Send(void* buffer, int size, int rank, int tag) {
    MPIV.pending++;

    if ((size_t) size <= INLINE) {
        // This can be inline, no need to copy them, but need to put in IMM.
        uint32_t imm = (tag); // FIXME: compress more data.
        MPIV.conn[rank].write_send((void*) buffer, (size_t) size,
                MPIV.sbuf.lkey(), 0, imm);
        // Done here.
        MPIV.pending--;
        return;
    }

    // First we need a packet, as we don't want to register.
    mpiv_packet* packet = get_freesbuf();

    if ((size_t) size <= SHORT) {
        // This is a short message, we send them immediately and do not yield
        // or create a request for it.

        packet->header.from = MPIV.me;
        packet->header.tag = tag;
        packet->header.type = SEND_SHORT;

        // Copy the buffer.
        memcpy(packet->egr.buffer, buffer, size);
        MPIV.conn[rank].write_send((void*) packet,
                (size_t) size + sizeof(mpiv_packet_header),
                MPIV.sbuf.lkey(), (void*) packet);
    } else {
        // This is rdz protocol, we notify the reciever with a ready_send.
        packet->header.from = MPIV.me;
        packet->header.tag = tag;
        packet->header.type = SEND_READY;
        MPIV.conn[rank].write_send((void*) packet,
                sizeof(mpiv_packet_header), MPIV.sbuf.lkey(), (void*) packet);

        // Meanwhile, if there is not matching, we wait in the buffer.
        // We also need to post a message to buffer to make sure we do not ran
        // out of resources and this thread never wake.

        // Now check our hash table to see if we find any matching.
        void* matching;
        MPIV_Request s(buffer, size, rank, tag);
        mpiv_key key = mpiv_make_key(rank, tag);

        if (!MPIV.rdztbl.find(key, matching)) {
            if (MPIV.rdztbl.insert(key, (void*) &s)) {
                s.wait();
                MPIV.rdztbl.erase(key);
                MPIV.pending--;
                return;
            }
            matching = MPIV.rdztbl[key];
        }

        mpiv_packet* p_ctx = (mpiv_packet*) matching;
        p_ctx->header.type = SEND_READY_FIN;
        packet = (mpiv_packet*) p_ctx;

        // Update the table, store the request.
        MPIV.rdztbl.update(key, (void*) &s);
        // printf("write rdma %d %d\n", rank, tag);

        // Write a RDMA.
        MPIV.conn[rank].write_rdma_signal(buffer, MPIV.heap.lkey(),
                (void*) packet->rdz.tgt_addr, packet->rdz.rkey, size,
                p_ctx, (uint32_t) tag);

        // Needs to wait, since buffer is not available until message is sent.
        s.wait();
        MPIV.rdztbl.erase(key);
    }

    MPIV.pending--;
}
#endif

#endif
