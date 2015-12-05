#ifndef SERVER_H_
#define SERVER_H_

#include <thread>

bool MPIV_Progress();
void mpiv_post_recv(mpiv_packet*);

class mpiv_server {
 public:
    mpiv_server() : stop_(false), done_init_(false) {
    }

    inline void init() {
        std::vector<rdmax::device> devs = rdmax::device::get_devices();
        assert(devs.size() > 0 && "Unable to find any ibv device");

        MPIV.dev_ctx = new device_ctx(devs.back());
        MPIV.dev_scq = std::move(MPIV.dev_ctx->create_cq(64*1024));
        MPIV.dev_rcq = std::move(MPIV.dev_ctx->create_cq(64*1024));

        // Create RDMA memory.
        int mr_flags =
            IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;

        // These are pinned memory.
        MPIV.sbuf = MPIV.dev_ctx->create_memory(sizeof(mpiv_packet) * NSBUF, mr_flags);
        MPIV.sbuf_alloc = new pinned_pool(MPIV.sbuf.ptr());
        MPIV.heap = MPIV.dev_ctx->create_memory((size_t) HEAP_SIZE, mr_flags);
        MPIV.heap_segment = std::move(mbuffer(
                    boost::interprocess::create_only, MPIV.heap.ptr(), (size_t) HEAP_SIZE));
        // mlock(MPIV.heap.ptr(), (size_t) HEAP_SIZE);
        // mlock(MPIV.sbuf.ptr(), (size_t) sizeof(mpiv_packet) * NSBUF);

        // Initialize connection.
        int rank, size;

        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        MPI_Comm_size(MPI_COMM_WORLD, &size);

        MPIV.me = rank;

        for (int i = 0; i < size; i++) {
            MPIV.conn.emplace_back(&MPIV.dev_scq, &MPIV.dev_rcq, MPIV.dev_ctx, &MPIV.sbuf, i);
        }

        /* PREPOST recv and allocate send queue. */
        for (int i = 0; i < NPREPOST; i++) {
            mpiv_post_recv((mpiv_packet*) MPIV.sbuf_alloc->allocate());
        }

        for (int i = 0; i < NSBUF - NPREPOST; i++) {
            mpiv_packet* packet = (mpiv_packet*) MPIV.sbuf_alloc->allocate();
            packet->header.type = SEND_SHORT;
            packet->header.from = rank;
            MPIV.squeue.push(packet);
        }
        done_init_ = true;
#if 0
        eventSetP = PAPI_NULL;
#endif
    }

    inline void serve() {
        poll_thread_ = std::thread([this] {
#if 0 
            PAPI_create_eventset(&eventSetP);
            PAPI_add_event(eventSetP, PAPI_L1_DCM);
            PAPI_add_event(eventSetP, PAPI_L2_DCM);
            PAPI_add_event(eventSetP, PAPI_L3_TCM);
            PAPI_start(eventSetP);
#endif
            uint8_t freq = 0;

            while (1) {
                while (MPIV_Progress()) {};
                if (freq++ == 0) {
                    if (this->stop_) break;
                }
            }
#if 0
            PAPI_stop(eventSetP, t1_valueP);
#endif
        });
    }

    inline void finalize() {
        stop_ = true;
        poll_thread_.join();
    }

 private:
    std::thread poll_thread_;
    volatile bool stop_;
    volatile bool done_init_;
};

#endif
