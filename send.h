#ifndef SEND_H_
#define SEND_H_

inline void MPIV_Send(void* buffer, int size, int rank, int tag) {

    if ((size_t) size <= INLINE) {
        // This can be inline, no need to copy them, but need to put in IMM.
        uint32_t imm = (tag); // FIXME: compress more data.
        MPIV.conn[rank].write_send((void*) buffer, (size_t) size,
                MPIV.sbuf.lkey(), 0, imm);
        // Done here.
        return;
    }

    // Otherwise we need a packet.
    mpiv_packet* packet;
    uint8_t count = 0;
    while(!MPIV.squeue.pop(packet)) {
        if (++count == 0) sched_yield();
    }
    packet->header.tag = tag;
    if ((size_t) size <= SHORT) {
        // This is a short message, we send them immediately and do not yield
        // or create a request for it.
        packet->header.type = SEND_SHORT;
        memcpy(packet->egr.buffer, buffer, size);
        MPIV.conn[rank].write_send((void*) packet,
                (size_t) size + sizeof(mpiv_packet_header),
                MPIV.sbuf.lkey(), (void*) packet);
    } else {
        // This is rdz protocol, we send a ready to send message with the size.
        device_memory* dm;
        if (!pinned.find(buffer, dm)) {
            dm = new device_memory{MPIV.dev_ctx->attach_memory(buffer, size,
                    IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ)};
            pinned.insert(buffer, dm);
        }

        packet->header.type = SEND_READY;
        packet->rdz.src_addr = (uintptr_t) buffer;
        packet->rdz.lkey = dm->lkey();
        packet->rdz.size = size;
        MPIV.conn[rank].write_send((void*) packet,
                sizeof(mpiv_packet_header) + sizeof(packet->rdz),
                MPIV.sbuf.lkey(), (void*) packet);
    }
}

#endif
