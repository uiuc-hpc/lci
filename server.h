#ifndef SERVER_H_
#define SERVER_H_

#include <thread>
#include "progress.h"

void mpiv_post_recv(mpiv_packet*);

typedef void(*p_ctx_handler)(mpiv_packet* p_ctx);
static p_ctx_handler handle[4];

static void mpiv_progress_init() {
    handle[SEND_SHORT] = mpiv_recv_short;
    handle[SEND_READY] = mpiv_recv_send_ready;
    handle[RECV_READY] = mpiv_recv_recv_ready;
    handle[SEND_READY_FIN] = mpiv_recv_send_ready_fin;
}

inline void mpiv_serve_recv(const ibv_wc& wc) {
    mpiv_packet* p_ctx = (mpiv_packet*) wc.wr_id;
#if 0
    if (wc.byte_len <= INLINE && wc.imm_data != 0) {
        // This is INLINE, we do not have header to check in some cases.
        mpiv_recv_inline(p_ctx, wc);
        return;
    } else
#endif
    {
        const auto& type = p_ctx->header.type;
        handle[type](p_ctx);
    }
}

inline void mpiv_serve_send(const ibv_wc& wc) {
    // Nothing to process, return.
    mpiv_packet* p_ctx = (mpiv_packet*) wc.wr_id;
    if (!p_ctx) return;
    const auto& type = p_ctx->header.type;
    if (type == SEND_READY_FIN) {
        mpiv_done_rdz_send(p_ctx);
    } else {
        MPIV.pk_mgr.new_packet(p_ctx);
    }
}

class mpiv_server {
 public:
    mpiv_server() : stop_(false), done_init_(false) {
    }

    inline void init() {
        std::vector<rdmax::device> devs = rdmax::device::get_devices();
        assert(devs.size() > 0 && "Unable to find any ibv device");

        dev_ctx_ = new device_ctx(devs.back());
        dev_scq_ = std::move(dev_ctx_->create_cq(64*1024));
        dev_rcq_ = std::move(dev_ctx_->create_cq(64*1024));

        // Create RDMA memory.
        int mr_flags =
            IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;

        // These are pinned memory.
        sbuf_ = dev_ctx_->create_memory(sizeof(mpiv_packet) * NSBUF, mr_flags);
        MPIV.sbuf_lkey = sbuf_.lkey();

        sbuf_alloc_ = new pinned_pool(sbuf_.ptr());
        heap_ = dev_ctx_->create_memory((size_t) HEAP_SIZE, mr_flags);

        MPIV.heap_rkey = heap_.rkey();
        MPIV.heap_lkey = heap_.lkey();
        MPIV.heap_segment = std::move(mbuffer(
                    boost::interprocess::create_only, heap_.ptr(), (size_t) HEAP_SIZE));

        // Initialize connection.
        int rank, size;

        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        MPI_Comm_size(MPI_COMM_WORLD, &size);

        MPIV.me = rank;

        for (int i = 0; i < size; i++) {
            MPIV.conn.emplace_back(&dev_scq_, &dev_rcq_, dev_ctx_, &sbuf_, i);
        }

        /* PREPOST recv and allocate send queue. */
        for (int i = 0; i < NPREPOST; i++) {
            mpiv_post_recv((mpiv_packet*) sbuf_alloc_->allocate());
        }

        for (int i = 0; i < NSBUF - NPREPOST; i++) {
            mpiv_packet* packet = (mpiv_packet*) sbuf_alloc_->allocate();
            MPIV.pk_mgr.new_packet(packet);
        }
        done_init_ = true;
#if 0
        eventSetP = PAPI_NULL;
#endif
    }

    inline void post_srq(mpiv_packet* p) {
      dev_ctx_->post_srq_recv((void*) p, (void*) p, sizeof(mpiv_packet), sbuf_.lkey());
    }

    inline bool progress() {
      initt(t);
      startt(t);
#if 0
      PAPI_read(eventSetP, t0_valueP);
#endif
      bool ret = (dev_rcq_.poll_once([](const ibv_wc& wc) {
            mpiv_serve_recv(wc);
            }));
      ret |= (dev_scq_.poll_once([](const ibv_wc& wc) {
            mpiv_serve_send(wc);
            }));
      stopt(t)
#if 0
        PAPI_read(eventSetP, t1_valueP);
      if (ret) {
        poll_timing += t;
        for (int j = 0; j<3; j++) {
          t_valueP[j] += (t1_valueP[j] - t0_valueP[j]);
        }
      }
#endif
      return ret;
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
                while (progress()) {};
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
        delete dev_ctx_;
        sbuf_.finalize();
        heap_.finalize();
        delete sbuf_alloc_;
    }

 private:
    std::thread poll_thread_;
    volatile bool stop_;
    volatile bool done_init_;
    device_ctx* dev_ctx_;
    device_cq dev_scq_;
    device_cq dev_rcq_;
    device_memory sbuf_;
    device_memory heap_;
    pinned_pool * sbuf_alloc_;
};

#endif
