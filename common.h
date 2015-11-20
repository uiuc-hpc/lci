#ifndef COMMON_H_
#define COMMON_H_
// static double timing = 0;

#define PACKET_SIZE (128*1024)
#define SHORT (PACKET_SIZE - sizeof(mpiv_packet_header))
#define INLINE 512

#define NSBUF 64
#define NPREPOST 16

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

struct mpiv {
    int me;
    device_ctx* dev_ctx;
    device_cq dev_scq;
    device_cq dev_rcq;
    device_memory sbuf;
    pinned_pool * sbuf_alloc;
    cuckoohash_map<int, void*> tbl;
    cuckoohash_map<int, void*> rdztbl;
    vector<connection> conn;
    vector<mpiv_packet*> prepost;
    boost::lockfree::queue<mpiv_packet*, boost::lockfree::capacity<NSBUF>> squeue;

    ~mpiv() {
        sbuf.finalize();
        delete dev_ctx;
        delete sbuf_alloc;
    }
};

thread_local cuckoohash_map<void*, device_memory*> pinned;

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
#endif

#endif
