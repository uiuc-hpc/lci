#ifndef MPIV_H_
#define MPIV_H_

#include <mpi.h>
#include <libcuckoo/cuckoohash_map.hh>
#include <vector>
#include <atomic>
#include <boost/lockfree/queue.hpp>
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>

#include "rdmax.h"

#define PAGE_SIZE 8192
#define PACKET_SIZE 4096

#define SHORT (PACKET_SIZE - sizeof(mpiv_packet_header))
#define INLINE 512

#define NSBUF 512
#define NRBUF 512

static double timing = 0;

using rdmax::device_ctx;
using rdmax::device_cq;
using rdmax::device_memory;
using rdmax::connection;

enum mpiv_packet_type {
    SEND_SHORT, RECV_SHORT,
    SEND_READY, RECV_READY,
    SEND_READY_FIN
};

// TODO(danghvu): can we piggypack this in the IMM field and drop the header?
struct mpiv_packet_header {
    mpiv_packet_type type;
    int from;
    int tag;
} __attribute__ ((aligned (8)));;

struct mpiv_packet {
    mpiv_packet_header header;
    union {
        struct {
            char buffer[SHORT];
        } egr;
        struct {
            uint32_t idx;
            uintptr_t src_addr;
            uint32_t lkey;
            uintptr_t tgt_addr;
            uint32_t rkey;
            uint32_t size;
        } rdz;
    };
};

struct pinned_pool {
    pinned_pool(void* ptr_) : ptr((uintptr_t) ptr_), last(0) {}

    uintptr_t ptr;
    std::atomic<size_t> last;

    void* allocate() {
        ptrdiff_t diff = (ptrdiff_t) (last.fetch_add(1) * PACKET_SIZE);
        return (void*) (ptr + diff);
    }
};

struct mpiv {
    int me;
    device_ctx* dev_ctx;
    device_cq dev_scq;
    device_cq dev_rcq;
    device_memory sbuf;
    device_memory rbuf;
    pinned_pool * sbuf_alloc;
    pinned_pool * rbuf_alloc;
    cuckoohash_map<int, void*> tbl;
    vector<connection> conn;
    vector<mpiv_packet*> prepost;
    boost::lockfree::queue<mpiv_packet*, boost::lockfree::capacity<NSBUF>> squeue;
};

thread_local cuckoohash_map<void*, device_memory*> pinned;

static mpiv MPIV;

inline void mpiv_post_recv(mpiv_packet* p) {
    MPIV.dev_ctx->post_srq_recv((void*) p,
        &(p->egr.buffer), SHORT, MPIV.rbuf.lkey());
}

inline void MPIV_Init(int &argc, char**& args) {
    MPI_Init(&argc, &args);

    std::vector<rdmax::device> devs = rdmax::device::get_devices();
    assert(devs.size() > 0 && "Unable to find any ibv device");

    MPIV.dev_ctx = new device_ctx(devs[0]);
    MPIV.dev_scq = std::move(MPIV.dev_ctx->create_cq(64*1024));
    MPIV.dev_rcq = std::move(MPIV.dev_ctx->create_cq(64*1024));

    // Create RDMA memory.
    int mr_flags =
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;

    // These are pinned memory.
    MPIV.sbuf = std::move(MPIV.dev_ctx->create_memory(PACKET_SIZE * NSBUF, mr_flags));
    MPIV.rbuf = std::move(MPIV.dev_ctx->create_memory(PACKET_SIZE * NRBUF, mr_flags));

    MPIV.sbuf_alloc = new pinned_pool(MPIV.sbuf.ptr());
    MPIV.rbuf_alloc = new pinned_pool(MPIV.rbuf.ptr());

    int rank, size;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    MPIV.me = rank;

    for (int i = 0; i < size; i++) {
        MPIV.conn.emplace_back(&MPIV.dev_scq, &MPIV.dev_rcq, MPIV.dev_ctx, &MPIV.sbuf, i);
    }

    // PREPOST recv and allocate send queue.
    for (int i = 0; i < NRBUF; i++) {
        MPIV.prepost.emplace_back((mpiv_packet*) MPIV.rbuf_alloc->allocate());
        MPIV.prepost[i]->header.type = RECV_SHORT;
        mpiv_post_recv(MPIV.prepost[i]);
    }

    for (int i = 0; i < NSBUF; i++) {
        mpiv_packet* packet = (mpiv_packet*) MPIV.sbuf_alloc->allocate();
        packet->header.type = SEND_SHORT;
        packet->header.from = rank;
        assert(MPIV.squeue.push(packet));
    }

    MPI_Barrier(MPI_COMM_WORLD);
}

inline void MPIV_Finalize() {
    MPI_Finalize();
}

static device_memory dm;

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

void mpiv_rdz_send(mpiv_packet* p_ctx, mpiv_packet* p, void* pinned_buffer) {
    // Attach the buffer (if not already).
    device_memory* dm;
    if (!pinned.find(pinned_buffer, dm)) {
        dm = new device_memory{MPIV.dev_ctx->attach_memory(pinned_buffer, p->rdz.size,
            IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ)};
        pinned.insert(pinned_buffer, dm);
        // printf("attached: %d\n", dm.rkey());
    }

    int from = p->header.from;

    // Need to write them back, setup as a RECV_READY.
    p->rdz.idx = p->header.tag; // make a pair of tag and rank perhaps ?
    p->rdz.tgt_addr = (uintptr_t) pinned_buffer;
    p->rdz.rkey = dm->rkey();
    p->header.from = MPIV.me;
    p->header.type = RECV_READY;

    p_ctx->header.type = RECV_READY;

    MPIV.conn[from].write_send((void*) p,
            sizeof(mpiv_packet_header) + sizeof(p->rdz),
            MPIV.rbuf.lkey(),
            p_ctx);
    // printf("Done send %p\n", p_ctx);
}


inline void MPIV_Recv2(void* buffer, int size, int rank, int tag) {
    void* local_buf;
    MPIV_Request s(buffer, size, rank, tag);

    // Find if the message has arrived, if not go and make a request.
    if (!MPIV.tbl.find(tag, local_buf)) {
        if (MPIV.tbl.insert(tag, (void*) &s)) {
            s.wait();
            return;
        } else {
            local_buf = MPIV.tbl[tag];
        }
    }
    mpiv_packet* p_ctx = (mpiv_packet*) local_buf;
    if ((uint32_t) size <= INLINE) {
        memcpy(buffer, p_ctx->egr.buffer, size);
        MPIV.tbl.erase(tag);
        mpiv_post_recv(p_ctx);
        return;
    }

    // Now we need packet.
    mpiv_packet* packet = (mpiv_packet*) p_ctx->egr.buffer;
    if ((uint32_t) size <= SHORT) {
        // Message has arrived, go and copy the message.
        memcpy(buffer, packet->egr.buffer, size);
        MPIV.tbl.erase(tag);
        mpiv_post_recv(p_ctx);
    } else {
        // Buffer has arrived, go and send it back.
        // printf("%p arrived, sending %p\n", buffer, p_ctx);
        mpiv_rdz_send(p_ctx, packet, buffer);
        MPIV.tbl.update(tag, (void*) &s);
        s.wait();
        // Now wait for the final message.
    }
}

inline void mpiv_recv_inline(mpiv_packet* p_ctx, const ibv_wc& wc) {
    uint32_t recv_size = wc.byte_len;
    int tag = wc.imm_data;
    void *sync = NULL;
    MPIV_Request* fsync = NULL;
    if (!MPIV.tbl.find(tag, sync)) {
        if (MPIV.tbl.insert(tag, (void*) p_ctx)) {
            // This will be handle by the thread,
            // so we return right away.
            return;
        }
        fsync = (MPIV_Request*) (void*) MPIV.tbl[tag];
    } else {
        fsync = (MPIV_Request*) sync;
    }
    memcpy(fsync->buffer, p_ctx->egr.buffer, recv_size);
    fsync->signal();
    MPIV.tbl.erase(tag);
    mpiv_post_recv(p_ctx);
}

inline void mpiv_recv_short(mpiv_packet* p_ctx,
        mpiv_packet* p, const ibv_wc& wc) {
    uint32_t recv_size = wc.byte_len - sizeof(mpiv_packet_header);
    int tag = p->header.tag;
    // printf("RECV size %d tag %d ctx %p pointer %p\n", recv_size, tag, p_ctx, p);

    void *sync = NULL;
    MPIV_Request* fsync = NULL;
    if (!MPIV.tbl.find(tag, sync)) {
        if (MPIV.tbl.insert(tag, (void*) p_ctx)) {
            // This will be handle by the thread,
            // so we return right away.
            return;
        }
        fsync = (MPIV_Request*) (void*) MPIV.tbl[tag];
    } else {
        fsync = (MPIV_Request*) sync;
    }

    memcpy(fsync->buffer, p->egr.buffer, recv_size);
    fsync->signal();
    MPIV.tbl.erase(tag);
    mpiv_post_recv(p_ctx);
}

inline void mpiv_recv_send_ready(mpiv_packet* p_ctx, mpiv_packet* p) {
    int tag = p->header.tag;
    // printf("%p recv send_ready \n", p_ctx);

    void *sync = NULL;
    MPIV_Request* fsync = NULL;
    if (!MPIV.tbl.find(tag, sync)) {
        if (MPIV.tbl.insert(tag, (void*) p_ctx)) {
            // printf("%p added \n", p_ctx);
            // This will be handle by the thread,
            // so we return right away.
            return;
        }
        fsync = (MPIV_Request*) (void*) MPIV.tbl[tag];
    } else {
        fsync = (MPIV_Request*) sync;
    }

    // printf("got buffer %p -> %p\n", fsync, fsync->buffer);
    mpiv_rdz_send(p_ctx, p, fsync->buffer);
}

inline void mpiv_recv_recv_ready(mpiv_packet* p_ctx, mpiv_packet* p) {
    // printf("sending: %p %d t: %p %d sz: %d\n", p->rdz.src_addr, p->rdz.lkey, p->rdz.tgt_addr,
    //    p->rdz.rkey, p->rdz.size);

    p_ctx->header.type = SEND_READY_FIN;
    MPIV.conn[p->header.from].write_rdma_signal((void*) p->rdz.src_addr, p->rdz.lkey,
        (void*) p->rdz.tgt_addr, p->rdz.rkey, p->rdz.size, p_ctx,
        p->rdz.idx);
}

inline void mpiv_recv_recv_ready_finalize(mpiv_packet* p_ctx, int tag) {
    // Now data is already ready in the buffer.
    void *sync = NULL;
    MPIV_Request* fsync = NULL;

    MPIV.tbl.find(tag, sync);
    //if ((uintptr_t) fsync->buffer != p->rdz.tgt_addr)
    // memcpy(fsync->buffer, (void*) p->rdz.tgt_addr, p->rdz.size);

    fsync = (MPIV_Request*) sync;
    fsync->signal();
    MPIV.tbl.erase(tag);
    mpiv_post_recv(p_ctx);
}

inline void MPIV_Progress() {
    // Poll recv queue.
    MPIV.dev_rcq.poll_once([](const ibv_wc& wc) {
        if (wc.opcode == IBV_WC_RECV_RDMA_WITH_IMM) {
            // FIXME: how do I get the rank ? From qpn probably ?
            // And how do I know this is the RECV_READY ?
            int tag = (int) wc.imm_data & 0xffffffff;
            mpiv_packet* p_ctx = (mpiv_packet*) wc.wr_id;
            mpiv_recv_recv_ready_finalize(p_ctx, tag);
        } else if (wc.imm_data != (uint32_t)-1) {
            // This is INLINE.
            mpiv_packet* p_ctx = (mpiv_packet*) wc.wr_id;
            mpiv_recv_inline(p_ctx, wc);
        } else {
            mpiv_packet* p_ctx = (mpiv_packet*) wc.wr_id;
            mpiv_packet* p = (mpiv_packet*) &(p_ctx->egr.buffer);
            if (p->header.type == SEND_SHORT) {
                // Return true if we need to return this p_ctx.
                mpiv_recv_short(p_ctx, p, wc);
            } else if (p->header.type == SEND_READY) {
                mpiv_recv_send_ready(p_ctx, p);
            } else if (p->header.type == RECV_READY) {
                mpiv_recv_recv_ready(p_ctx, p);
            } else {
                assert(0 && "Invalid packet or not implemented");
            }
        }
    });

    // Poll send queue.
    MPIV.dev_scq.poll_once([](const ibv_wc& wc) {
        if (wc.wr_id == 0) {
            // This is an INLINE, do nothing.
            return;
        }
        mpiv_packet* packet = (mpiv_packet*) wc.wr_id;
        // Return this free packet to the queue.
        if (packet->header.type == SEND_SHORT) {
            assert(MPIV.squeue.push(packet));
        } else if (packet->header.type == SEND_READY) {
            assert(MPIV.squeue.push(packet));
        } else if (packet->header.type == RECV_READY) {
            // Done sending the tgt_addr, return the packet.
            mpiv_post_recv(packet);
        } else if (packet->header.type == SEND_READY_FIN) {
            // finished rdz protocol for send.
            mpiv_post_recv(packet);
        } else {
            assert(0 && "Invalid packet or not implemented");
        }
    });
}

inline void MPI_Progress() {
    MPI_Status stat;
    int flag = 0;
    MPI_Iprobe(1, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &stat);

    // nothing.
    if (!flag) return;

    int tag = stat.MPI_TAG;
    void *sync = NULL;
    if (!MPIV.tbl.find(tag, sync)) {
        // unmatched recv, allocate a buffer for it and continue.
        int size = 0;
        MPI_Get_count(&stat, MPI_BYTE, &size);
        void* buf = std::malloc(size);
        MPI_Recv(buf, size, MPI_BYTE, stat.MPI_SOURCE, tag,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        if (!MPIV.tbl.insert(tag, buf)) {
            MPIV_Request* fsync = (MPIV_Request*) (void*) MPIV.tbl[tag];
            memcpy(fsync->buffer, buf, size);
            fsync->signal();
            free(buf);
            MPIV.tbl.erase(tag);
        } else {
            // this branch is handle
            // by the thread.
        }
    } else {
        MPIV_Request* fsync = (MPIV_Request*) sync;
        MPI_Recv(fsync->buffer, fsync->size, MPI_BYTE, 1, tag,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        fsync->signal();
        MPIV.tbl.erase(tag);
    }
}

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
