#ifndef COMMON_H_
#define COMMON_H_

#include "config.h"

/** Setup hash table */
typedef uint64_t mpiv_key;
struct mpiv_packet;
struct MPIV_Request;

union mpiv_value {
    void* v;
    mpiv_packet* packet;
    MPIV_Request* request;
};

#ifdef USE_LF
extern "C" {
    #include "lf_hash.h"
}
typedef qt_hash mpiv_hash_tbl;
#endif

#ifdef USE_ARRAY
typedef mpiv_value* mpiv_hash_tbl;
#endif

#ifdef USE_COCK
#include <libcuckoo/cuckoohash_map.hh>
struct my_hash {
  size_t operator()(uint64_t __x) const { return __x; }
};

typedef cuckoohash_map<mpiv_key, mpiv_value, my_hash> mpiv_hash_tbl;
#endif

constexpr mpiv_key mpiv_make_key(const int& rank, const int& tag) {
    return ((uint64_t) rank << 32) | tag;
}

#ifdef USE_TIMING
/** Setup timing */
static double tbl_timing;
static double signal_timing;
static double memcpy_timing;
static double wake_timing;
static double post_timing;
static double misc_timing;
static double poll_timing;
static double rdma_timing;

static int eventSetP;
static long long t_valueP[3], t0_valueP[3], t1_valueP[3];

#define startt(x) { x -= MPIV_Wtime(); }
#define stopt(x) { x += MPIV_Wtime(); }
#define resett(x) { x = 0; }
#else
#define startt(x) {}
#define stopt(x) {}
#define resett(x) {}
#endif

/** Setup struct and RDMAX */
using rdmax::device_ctx;
using rdmax::device_cq;
using rdmax::device_memory;
using rdmax::connection;

enum mpiv_packet_type {
    SEND_SHORT, SEND_READY, RECV_READY,
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
            uintptr_t sreq;
            uintptr_t rreq;
            uintptr_t tgt_addr;
            uint32_t rkey;
            uint32_t size;
        } rdz;
    };
} __attribute__ ((aligned(64)));

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
     boost::interprocess::rbtree_best_fit< boost::interprocess::mutex_family,
        void*, 64>,
     boost::interprocess::iset_index > mbuffer;

class mpiv_server;

struct mpiv {
    int me;
    device_ctx* dev_ctx;
    device_cq dev_scq;
    device_cq dev_rcq;

    device_memory sbuf;
    pinned_pool * sbuf_alloc;

    device_memory heap;
    mbuffer heap_segment;

    mpiv_hash_tbl tbl; // 32-bit rank | 32-bit tag

    vector<connection> conn;
    boost::lockfree::stack<mpiv_packet*, boost::lockfree::capacity<NSBUF>> squeue;

    mpiv_server *server;

    ~mpiv() {
        sbuf.finalize();
        heap.finalize();
        delete dev_ctx;
        delete sbuf_alloc;
    }
} __attribute__ ((aligned(64)));

static mpiv MPIV;

double MPIV_Wtime() {
    using namespace std::chrono;
    return duration_cast<duration<double> >(
        high_resolution_clock::now().time_since_epoch()).count();
}

mpiv_packet* mpiv_getpacket() {
    uint8_t count = 0;
    mpiv_packet* packet;
    while(!MPIV.squeue.pop(packet)) {
        if (++count == 0) fult_yield();
    }
    return packet;
}

void* mpiv_malloc(size_t size) {
    return MPIV.heap_segment.allocate((size_t) size);
}

void mpiv_free(void* ptr) {
    MPIV.heap_segment.deallocate(ptr);
}

void mpiv_send_recv_ready(MPIV_Request* sreq, MPIV_Request* rreq) {
    // Need to write them back, setup as a RECV_READY.
    char data[64];
    mpiv_packet* p = (mpiv_packet*) data;
    p->header = {RECV_READY, MPIV.me, rreq->tag};
    p->rdz = {(uintptr_t) sreq, (uintptr_t) rreq,
        (uintptr_t) rreq->buffer, MPIV.heap.rkey(), (uint32_t) rreq->size};
    MPIV.conn[rreq->rank].write_send((void*) p, 64, 0, 0);
}

#endif
