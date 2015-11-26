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

    mpiv_packet* packet = get_freesbuf();

    // First we need a packet, as we don't want to register.
    if ((size_t) size <= SHORT) {
        packet->header = {SEND_SHORT, MPIV.me, tag};
        // This is a short message, we send them immediately and do not yield
        // or create a request for it.
        
        // Copy the buffer.
        memcpy(packet->egr.buffer, buffer, size);
        MPIV.conn[rank].write_send((void*) packet,
                (size_t) size + sizeof(mpiv_packet_header),
                MPIV.sbuf.lkey(), (void*) packet);
    } else {
        packet->header = {SEND_READY, MPIV.me, tag};
        MPIV_Request s(buffer, size, rank, tag);
        mpiv_key key = mpiv_make_key(rank, tag);
        MPIV.rdztbl.insert(key, (void*) &s);

        // This is rdz protocol, we notify the reciever with a ready_send.
        MPIV.conn[rank].write_send((void*) packet,
                sizeof(mpiv_packet_header), MPIV.sbuf.lkey(), packet);
        // Needs to wait, since buffer is not available until message is sent.
        timing -= MPI_Wtime();
        s.wait();
    }

    MPIV.pending--;
}
#endif

#endif
