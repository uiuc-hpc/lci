#ifndef COMMON_H_
#define COMMON_H_

using namespace boost::interprocess;

#define PACKET_SIZE (32*1024)
#define SHORT (PACKET_SIZE - sizeof(mpiv_packet_header))
#define INLINE 512

#define NSBUF 64
#define NPREPOST 16

#define HEAP_SIZE 4*1024*1024*1024

using rdmax::device_ctx;
using rdmax::device_cq;
using rdmax::device_memory;
using rdmax::connection;

enum mpiv_packet_type {
    SEND_SHORT, RECV_SHORT,
    SEND_READY, RECV_READY,
    SEND_READY_FIN
};

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
            uintptr_t tgt_addr;
            uint32_t rkey;
            uint32_t size;
        } rdz;
    };
} __attribute__ ((aligned (8)));

struct pinned_pool {
    pinned_pool(void* ptr_) : ptr((uintptr_t) ptr_), last(0) {}

    uintptr_t ptr;
    std::atomic<size_t> last;

    void* allocate() {
        ptrdiff_t diff = (ptrdiff_t) (last.fetch_add(1) * PACKET_SIZE);
        return (void*) (ptr + diff);
    }
};

typedef basic_managed_external_buffer< char,rbtree_best_fit< mutex_family >,iset_index > mbuffer;
typedef uint64_t mpiv_key;

struct mpiv {
    int me;
    device_ctx* dev_ctx;
    device_cq dev_scq;
    device_cq dev_rcq;
    device_memory sbuf;

    device_memory heap;
    mbuffer heap_segment;

    pinned_pool * sbuf_alloc;
    cuckoohash_map<mpiv_key, void*> tbl; // 32-bit rank | 32-bit tag
    cuckoohash_map<uint64_t, void*> rdztbl; // 32-bit rank | 32-bit tag

    vector<connection> conn;
    vector<mpiv_packet*> prepost;
    boost::lockfree::queue<mpiv_packet*, boost::lockfree::capacity<NSBUF>> squeue;
    std::atomic<int> pending;

    ~mpiv() {
        sbuf.finalize();
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

void MPIV_Flush() {
    while (MPIV.pending) {};
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

#endif

#endif
