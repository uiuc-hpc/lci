#ifndef COMMON_H_
#define COMMON_H_

#define PACKET_SIZE (16*1024)
#define SHORT (PACKET_SIZE - sizeof(mpiv_packet_header))
#define INLINE 512

#define NSBUF 32
#define NPREPOST 16

#define HEAP_SIZE 4*1024*1024*1024

using rdmax::device_ctx;
using rdmax::device_cq;
using rdmax::device_memory;
using rdmax::connection;

enum mpiv_packet_type {
    SEND_SHORT, 
    SEND_READY, RECV_READY,
    SEND_READY_FIN
};

struct mpiv_packet_header {
    mpiv_packet_type type;
    int from;
    int tag;
} __attribute__ ((aligned));

struct mpiv_packet {
    mpiv_packet_header header;
    union {
        struct {
            char buffer[SHORT];
        } egr;
        struct {
            uintptr_t idx;
            uintptr_t tgt_addr;
            uint32_t rkey;
            uint32_t size;
        } rdz;
    };
} __attribute__ ((aligned));

struct pinned_pool {
    pinned_pool(void* ptr_) : ptr((uintptr_t) ptr_), last(0) {}

    uintptr_t ptr;
    std::atomic<size_t> last;

    void* allocate() {
        ptrdiff_t diff = (ptrdiff_t) (last.fetch_add(1) * sizeof(mpiv_packet));
        return (void*) (ptr + diff);
    }
};

typedef boost::interprocess::basic_managed_external_buffer< char,
     boost::interprocess::rbtree_best_fit< boost::interprocess::mutex_family >,
     boost::interprocess::iset_index > mbuffer;
typedef uint64_t mpiv_key;

struct mpiv {
    int me;
    device_ctx* dev_ctx;
    device_cq dev_scq;
    device_cq dev_rcq;

    device_memory sbuf;
    pinned_pool * sbuf_alloc;

    device_memory heap;
    mbuffer heap_segment;

    cuckoohash_map<mpiv_key, void*> tbl; // 32-bit rank | 32-bit tag

    vector<connection> conn;
    boost::lockfree::stack<mpiv_packet*, boost::lockfree::capacity<NSBUF>> squeue;

    ~mpiv() {
        sbuf.finalize();
        heap.finalize();
        delete dev_ctx;
        delete sbuf_alloc;
    }
};

static mpiv MPIV;

#ifndef USING_ABT
mpiv_packet* get_freesbuf() {
    uint8_t count = 0;
    mpiv_packet* packet;
    while(!MPIV.squeue.pop(packet)) {
        if (++count == 0) fult_yield();
    }
    return packet;
}

void* mpiv_malloc(size_t size) {
    return (void*) MPIV.heap_segment.allocate(size);
}

void mpiv_free(void* ptr) {
    MPIV.heap_segment.deallocate(ptr);
}


inline mpiv_key mpiv_make_key(int rank, int tag) {
    return ((uint64_t) rank << 32) | tag;
}

using namespace std::chrono;

inline double MPIV_Wtime() {
    return duration_cast<duration<double> >(
        high_resolution_clock::now().time_since_epoch()).count();
}

static double timing = 0;

void mpiv_send_recv_ready(uintptr_t idx, MPIV_Request* req) {
    // Need to write them back, setup as a RECV_READY.
    mpiv_packet* p = get_freesbuf();
    p->header = {RECV_READY, MPIV.me, req->tag};
    p->rdz = {idx, (uintptr_t) req->buffer, MPIV.heap.rkey(), (uint32_t) req->size};

    // printf("SEND RECV_READY %p %d %d\n", req->buffer, MPIV.heap.rkey(), req->size);
    MPIV.conn[req->rank].write_send((void*) p,
            sizeof(mpiv_packet_header) + sizeof(p->rdz),
            0, 0);
    MPIV.squeue.push(p);
}

#endif

#endif
