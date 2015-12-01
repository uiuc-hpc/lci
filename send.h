#ifndef SEND_H_
#define SEND_H_

#ifndef USING_ABT

void MPIV_Send(void* buffer, int size, int rank, int tag) {
    if ((size_t) size <= INLINE) {
        // This can be inline, no need to copy them, but need to put in IMM.
        uint32_t imm = (tag); // FIXME: compress more data.
        MPIV.conn[rank].write_send((void*) buffer, (size_t) size,
                MPIV.sbuf.lkey(), 0, imm);
        // Done here.
        return;
    }

    // First we need a packet, as we don't want to register.
    if ((size_t) size <= SHORT) {
        mpiv_packet* packet = get_freesbuf();
        packet->header = {SEND_SHORT, MPIV.me, tag};
        // This is a short message, we send them immediately and do not yield
        // or create a request for it.
        
        // Copy the buffer.
        memcpy(packet->egr.buffer, buffer, size);
        MPIV.conn[rank].write_send((void*) packet,
                (size_t) size + sizeof(mpiv_packet_header),
                MPIV.sbuf.lkey(), (void*) packet);
    } else {
        char data[64];
        mpiv_packet *p = (mpiv_packet*) data;
        MPIV_Request s(buffer, size, rank, tag);
        // char data[sizeof(mpiv_packet_header) + sizeof(p->rdz)];

        p->header = {SEND_READY, MPIV.me, tag};
        p->rdz.sreq = (uintptr_t) &s;

        // This is rdz protocol, we notify the reciever with a ready_send.
        MPIV.conn[rank].write_send((void*) p, 64, 0, 0);
        // Wait until got recv_ready.
        s.wait();
    }
}
#endif

#endif
