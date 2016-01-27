#ifndef SEND_H_
#define SEND_H_

void MPIV_Send(void* buffer, size_t size, int rank, int tag) {
    // First we need a packet, as we don't want to register.
    if ((size_t) size <= SHORT) {
        mpiv_packet* packet = MPIV.pk_mgr.get_packet(SEND_SHORT, MPIV.me, tag);
        // This is a short message, we send them immediately and do not yield
        // or create a request for it.
        // Copy the buffer.
        memcpy(packet->egr.buffer, buffer, size);
        MPIV.ctx.conn[rank].write_send((void*) packet,
                ALIGNED64((size_t) size + sizeof(mpiv_packet_header)),
                MPIV.ctx.sbuf_lkey, (void*) packet);
    } else {
        char data[64];
        MPIV_Request s(buffer, size, rank, tag);
        MPIV_Recv(data, 64, rank, 1 << 31 | tag);
        startt(rdma_timing);
        mpiv_packet* p = MPIV.pk_mgr.get_packet(data, SEND_READY_FIN, MPIV.me, tag);
        p->rdz.sreq = (uintptr_t) &s;
        MPIV.ctx.conn[rank].write_rdma(buffer, MPIV.ctx.heap_lkey,
            (void*) p->rdz.tgt_addr, p->rdz.rkey, size, p);
        MPIV.ctx.conn[rank].write_send(p, 64, 0, 0);
        MPIV_Wait(s);
    }
}

#endif
