#ifndef SERVER_H_
#define SERVER_H_

#include <thread>
#include <iostream>
#include <memory>

#include "packet_manager.h"
#include "profiler.h"
#include "affinity.h"

using std::unique_ptr;

void mpiv_serve_recv(const ibv_wc& wc);
void mpiv_serve_send(const ibv_wc& wc);
void mpiv_post_recv(mpiv_packet*);

double MPIV_Wtime();

struct pinned_pool {
  pinned_pool(void* ptr_) : ptr((uintptr_t)ptr_), last(0) {}

  uintptr_t ptr;
  std::atomic<size_t> last;

  void* allocate() {
    ptrdiff_t diff = (ptrdiff_t)(last.fetch_add(1) * sizeof(mpiv_packet));
    return (void*)(ptr + diff);
  }
};

class mpiv_server {
 public:
  mpiv_server() : stop_(false), done_init_(false) {}

  inline void init(mpiv_ctx& ctx, vector<local_pk_pool*>& localpk,
                   packet_manager& pkpool, int& rank, int& size) {
#ifdef USE_AFFI
    affinity::set_me_to(0);
#endif
    std::vector<rdmax::device> devs = rdmax::device::get_devices();
    assert(devs.size() > 0 && "Unable to find any ibv device");

    dev_ctx_ =
        std::move(std::unique_ptr<device_ctx>(new device_ctx(devs.back())));
    dev_scq_ = std::move(dev_ctx_->create_cq(64 * 1024));
    dev_rcq_ = std::move(dev_ctx_->create_cq(64 * 1024));

    // Create RDMA memory.
    int mr_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
                   IBV_ACCESS_REMOTE_WRITE;

    // These are pinned memory.
    sbuf_ = dev_ctx_->create_memory(
        sizeof(mpiv_packet) * (MAX_SEND + MAX_RECV + 2), mr_flags);
    ctx.sbuf_lkey = sbuf_.lkey();

    sbuf_alloc_ =
        std::move(std::unique_ptr<pinned_pool>(new pinned_pool(sbuf_.ptr())));
    heap_ = dev_ctx_->create_memory((size_t)HEAP_SIZE, mr_flags);

    ctx.heap_rkey = heap_.rkey();
    ctx.heap_lkey = heap_.lkey();
    ctx.heap_segment = std::move(mbuffer(boost::interprocess::create_only,
                                         heap_.ptr(), (size_t)HEAP_SIZE));

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    for (int i = 0; i < size; i++) {
      ctx.conn.emplace_back(&dev_scq_, &dev_rcq_, dev_ctx_.get(), &sbuf_, i);
    }

    // Prepare the packet_mgr and prepost some packet.
    for (int i = 0; i < MAX_SEND; i++) {
      pkpool.new_packet((mpiv_packet*)sbuf_alloc_->allocate());
    }

    for (int i = 0; i < MAX_RECV; i++) {
      pkpool.new_packet((mpiv_packet*)sbuf_alloc_->allocate());
    }

    pk_mgr_ptr = &pkpool;
    pk_mgr_local_ptr = &localpk;
    done_init_ = true;
  }

  inline void post_srq(mpiv_packet* p) {
    if (p == NULL) return;
    recv_posted_++;
    dev_ctx_->post_srq_recv((void*)p, (void*)p, sizeof(mpiv_packet),
                            sbuf_.lkey());
  }

  inline bool progress() {  // profiler& p, long long& r, long long &s) {
    initt(t);
    startt(t);
    bool ret = (dev_rcq_.poll_once([this](const ibv_wc& wc) {
      recv_posted_--;
      mpiv_serve_recv(wc);
    }));
    ret |= (dev_scq_.poll_once([](const ibv_wc& wc) { mpiv_serve_send(wc); }));
    stopt(t)
        // Make sure we always have enough packet, but do not block.
        if (recv_posted_ < MAX_RECV) {
      auto* p = pk_mgr_ptr->get_packet_nb();
      if (p != NULL) {
        recv_posted_++;
        dev_ctx_->post_srq_recv((void*)p, (void*)p, sizeof(mpiv_packet),
                                sbuf_.lkey());
      } else if (recv_posted_ < MAX_RECV / 2) {
        // steal from local pool.
        for (auto& localpool : *pk_mgr_local_ptr) {
          post_srq(localpool->steal_packet());
        }
      }
    }
    return ret;
  }

  inline void serve() {
    poll_thread_ = std::thread([this] {
#ifdef USE_AFFI
      affinity::set_me_to_last();
#endif

#ifdef USE_PAPI
      profiler server({PAPI_L1_DCM});
      server.start();
#endif

      while (xunlikely(!this->stop_)) {
        while (progress()) {
        };
      }

#ifdef USE_PAPI
      server.stop();
      server.print();
#endif
    });
  }

  inline void finalize() {
    stop_ = true;
    poll_thread_.join();
    dev_scq_.finalize();
    dev_rcq_.finalize();
    sbuf_.finalize();
    heap_.finalize();
  }

 private:
  std::thread poll_thread_;
  volatile bool stop_;
  volatile bool done_init_;
  unique_ptr<device_ctx> dev_ctx_;
  device_cq dev_scq_;
  device_cq dev_rcq_;
  device_memory sbuf_;
  device_memory heap_;
  int recv_posted_;
  unique_ptr<pinned_pool> sbuf_alloc_;
  packet_manager* pk_mgr_ptr;
  vector<local_pk_pool*>* pk_mgr_local_ptr;
};

#endif
