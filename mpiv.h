#ifndef MPIV_H_
#define MPIV_H_

#include <mpi.h>
#include <libcuckoo/cuckoohash_map.hh>
#include <vector>
#include <atomic>
#include <boost/lockfree/queue.hpp>

#include "rdmax.h"

#define PACKET_SIZE 4096
#define SHORT (PACKET_SIZE - sizeof(mpiv_packet_header))
#define NSBUF 512
#define NRBUF 512

using rdmax::device_ctx;
using rdmax::device_cq;
using rdmax::device_memory;
using rdmax::connection;

enum mpiv_packet_type {
    SEND_SHORT, RECV_SHORT
};

// TODO(danghvu): can we piggypack this in the IMM field and drop the header?
struct mpiv_packet_header {
    mpiv_packet_type type;
    int from;
    int tag;
};

struct mpiv_packet {
    mpiv_packet_header header;
    char buffer[SHORT];
};

struct buffer {
    buffer(void* ptr_) : ptr((uintptr_t) ptr_), last(0) {}

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
    buffer* sbuf_alloc;
    buffer* rbuf_alloc;
    cuckoohash_map<int, void*> tbl;
    vector<connection> conn;
    vector<mpiv_packet*> prepost;
    boost::lockfree::queue<mpiv_packet*, boost::lockfree::capacity<NSBUF>> squeue;
};

static mpiv MPIV;

inline void mpiv_post_recv(mpiv_packet* p) {
    assert(p->header.type == RECV_SHORT && "something wrong");
    MPIV.dev_ctx->post_srq_recv((void*) p,
        &(p->buffer), SHORT, MPIV.rbuf.lkey());
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

    MPIV.sbuf = std::move(MPIV.dev_ctx->create_memory(PACKET_SIZE * NSBUF, mr_flags));
    MPIV.rbuf = std::move(MPIV.dev_ctx->create_memory(PACKET_SIZE * NRBUF, mr_flags));
    MPIV.sbuf_alloc = new buffer(MPIV.sbuf.ptr());
    MPIV.rbuf_alloc = new buffer(MPIV.rbuf.ptr());

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

inline void MPIV_Send(void* buffer, int size, int rank, int tag) {
    mpiv_packet* packet;
    uint8_t count = 0;
    while(!MPIV.squeue.pop(packet)) {
        if (++count == 0) sched_yield();
    }

    packet->header.tag = tag;
    if (((size_t) size) <= SHORT) {
        // This is a short message, we send them immediately and do not yield
        // or create a request for it.
        memcpy(packet->buffer, buffer, size);
        MPIV.conn[rank].write_send((void*) packet,
                (size_t) size + sizeof(mpiv_packet_header),
                MPIV.sbuf.lkey(), (void*) packet);
    } else {
        assert( 0 && "Not implemented");
    }
}

inline void MPIV_Recv2(void* buffer, int size, int rank, int tag) {
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
    mpiv_packet* p_ctx = (mpiv_packet*) local_buf;
    mpiv_packet* packet = (mpiv_packet*) p_ctx->buffer;
    memcpy(buffer, packet->buffer, size);
    MPIV.tbl.erase(tag);
    mpiv_post_recv(p_ctx);
}

inline void mpiv_recv_short(mpiv_packet* p_ctx, const ibv_wc& wc) {
    mpiv_packet* p = (mpiv_packet*) &(p_ctx->buffer);
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

    memcpy(fsync->buffer, p->buffer, recv_size);
    fsync->signal();
    MPIV.tbl.erase(tag);
    mpiv_post_recv(p_ctx);
}

inline void MPIV_Progress() {
    MPIV.dev_rcq.poll_once([](const ibv_wc& wc) {
        mpiv_packet* p_ctx = (mpiv_packet*) wc.wr_id;
        if (p_ctx->header.type == RECV_SHORT) {
            mpiv_recv_short(p_ctx, wc);
        } else {
            assert(0 && "Invalid packet or not implemented");
        }
    });

    MPIV.dev_scq.poll_once([](const ibv_wc& wc) {
        mpiv_packet* packet = (mpiv_packet*) wc.wr_id;
        // Return this free packet to the queue.
        if (packet->header.type == SEND_SHORT) {
            assert(MPIV.squeue.push(packet));
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
