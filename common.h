#ifndef COMMON_H_
#define COMMON_H_

#include "config.h"
#include <stdexcept>

#define ALIGNED64(x) (((x)+63)/64*64)

/** Setup hash table */
typedef uint64_t mpiv_key;
struct mpiv_packet;
struct MPIV_Request;

constexpr mpiv_key mpiv_make_key(const int& rank, const int& tag) {
    return (((uint64_t) rank << 32) | tag);
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

#define initt(x) { double x = 0; }
#define startt(x) { x -= MPIV_Wtime(); }
#define stopt(x) { x += MPIV_Wtime(); }
#define resett(x) { x = 0; }
#else
#define initt(x) {}
#define startt(x) {}
#define stopt(x) {}
#define resett(x) {}
#endif

/** Setup struct and RDMAX */
using rdmax::device_ctx;
using rdmax::device_cq;
using rdmax::device_memory;
using rdmax::connection;

#include "hashtbl.h"

enum mpiv_packet_type {
    SEND_SHORT, SEND_READY, RECV_READY,
    SEND_READY_FIN
};

struct mpiv_packet_header {
    mpiv_packet_type type;
    int from;
    int tag;
} __attribute__((aligned(8)));

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

class packet_manager final {
 public:
  inline mpiv_packet* get_packet() {
    mpiv_packet* packet;
    if (!squeue_.pop(packet)) {
        throw new std::runtime_error("Not enough buffer, consider increasing concurrency level");
    }
    return packet;
  }

  inline mpiv_packet* get_packet(mpiv_packet_type packet_type, int rank, int tag) {
    mpiv_packet* packet = get_packet();
    packet->header = {packet_type, rank, tag};
    return packet;
  }

  inline mpiv_packet* get_packet(char buf[], mpiv_packet_type packet_type, int rank, int tag) {
    mpiv_packet* packet = reinterpret_cast<mpiv_packet*>(buf);
    packet->header = {packet_type, rank, tag};
    return packet;
  }

  inline void new_packet(mpiv_packet* packet) {
    if (!squeue_.push(packet)) {
        throw new std::runtime_error("Fatal error, insert more than possible packets into manager");
    }
  }

 private:
  boost::lockfree::stack<mpiv_packet*, boost::lockfree::capacity<NSBUF>> squeue_;
};

class memory_manager final {
 public:
 private:
};

struct mpiv {
    int me;
    vector<connection> conn;

    mbuffer heap_segment;
    mpiv_hash_tbl tbl;

    packet_manager pk_mgr;
    mpiv_server *server;

    uint32_t sbuf_lkey;
    uint32_t heap_rkey;
    uint32_t heap_lkey;

    ~mpiv() {
    }
} __attribute__ ((aligned(64)));

static mpiv MPIV;

double MPIV_Wtime() {
    using namespace std::chrono;
    return duration_cast<duration<double> >(
        high_resolution_clock::now().time_since_epoch()).count();
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
    mpiv_packet* p = MPIV.pk_mgr.get_packet(data, RECV_READY, MPIV.me, rreq->tag);
    p->rdz = {(uintptr_t) sreq, (uintptr_t) rreq,
        (uintptr_t) rreq->buffer, MPIV.heap_rkey, (uint32_t) rreq->size};
    MPIV.conn[rreq->rank].write_send((void*) p, 64, 0, 0);
}


#endif
