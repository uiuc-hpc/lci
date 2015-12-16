#ifndef SEND_H_
#define SEND_H_

void MPIV_Send(void* buffer, size_t size, int rank, int tag) {
    // First we need a packet, as we don't want to register.
    if ((size_t) size <= SHORT) {
        mpiv_packet* packet = mpiv_getpacket();
        packet->header = {SEND_SHORT, MPIV.me, tag};
        // This is a short message, we send them immediately and do not yield
        // or create a request for it.
        // Copy the buffer.
        memcpy(packet->egr.buffer, buffer, size);
        MPIV.conn[rank].write_send((void*) packet,
                ALIGNED64((size_t) size + sizeof(mpiv_packet_header)),
                MPIV.sbuf.lkey(), (void*) packet);
    } else {
        char data[64];
        mpiv_packet *p = (mpiv_packet*) data;
        MPIV_Request s(buffer, size, rank, tag);
        MPIV_Recv((void*) p, 64, rank, 1 << 31 | tag);
        startt(rdma_timing);
        p->header = {SEND_READY_FIN, MPIV.me, tag};
        p->rdz.sreq = (uintptr_t) &s;
        MPIV.conn[rank].write_rdma(buffer, MPIV.heap.lkey(),
            (void*) p->rdz.tgt_addr, p->rdz.rkey, size, p);
        MPIV.conn[rank].write_send(p, 64, 0, 0);
        MPIV_Wait(s);
    }
}

#endif
