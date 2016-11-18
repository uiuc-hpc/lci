#ifndef SERVER_RDMAX_H_
#define SERVER_RDMAX_H_

class ServerRdmax : ServerBase {
 public:
  ServerRdmax() : stop_(false), done_init_(false) {}
  MV_INLINE void init(mv_pp* pkpool, int& rank, int& size);
  MV_INLINE void post_recv(packet* p);
  MV_INLINE void serve();
  MV_INLINE void finalize();
  MV_INLINE void write_send(int rank, void* buf, size_t size, void* ctx);
  MV_INLINE void write_rma(int rank, void* from, uint32_t lkey, void* to, uint32_t rkey,
                        size_t size, void* ctx);
  MV_INLINE void write_rma_signal(int rank, void* from, uint32_t lkey, void* to, uint32_t rkey,
                        size_t size, uint32_t sid, void* ctx);
  MV_INLINE void* allocate(size_t s);
  MV_INLINE void deallocate(void* ptr);
  MV_INLINE uint32_t heap_rkey() { return heap_.rkey(); }
  MV_INLINE uint32_t heap_lkey() { return heap_.lkey(); }
  MV_INLINE uint32_t heap_rkey(int node) { return conn[node].rkey(); }
  MV_INLINE uint32_t sbuf_lkey() { return sbuf_.lkey(); }
  MV_INLINE uint32_t sbuf_rkey() { return sbuf_.rkey(); }

 private:
  MV_INLINE bool progress();
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
  mv_pp* pkpool;
  mbuffer heap_segment;
  vector<connection> conn;
};

void ServerRdmax::init(mv_pp* pkpool, int& rank, int& size) {
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
  int mr_flags =
      IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;

  // These are pinned memory.
  sbuf_ = dev_ctx_->create_memory(sizeof(packet) * (MAX_SEND + MAX_RECV + 2),
                                  mr_flags);

  sbuf_alloc_ =
      std::move(std::unique_ptr<pinned_pool>(new pinned_pool(sbuf_.ptr())));
  heap_ = dev_ctx_->create_memory((size_t)HEAP_SIZE, mr_flags);

  heap_segment = std::move(mbuffer(boost::interprocess::create_only,
                                   heap_.ptr(), (size_t)HEAP_SIZE));

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  for (int i = 0; i < size; i++) {
    conn.emplace_back(&dev_scq_, &dev_rcq_, dev_ctx_.get(), &heap_, i);
  }

  // Prepare the packet_mgr and prepost some packet.
  for (int i = 0; i < MAX_SEND + MAX_RECV; i++) {
    mv_pp_free(pkpool, (packet*)sbuf_alloc_->allocate());
  }
  this->pkpool = pkpool;
  this->recv_posted_ = 0;
  done_init_ = true;
}

MV_INLINE void ServerRdmax::post_recv(packet* p) {
  if (p == NULL) return;
  recv_posted_++;
  dev_ctx_->post_srq_recv((void*)p, (void*)p, sizeof(packet), sbuf_.lkey());
}

MV_INLINE bool ServerRdmax::progress() {  // profiler& p, long long& r, long long &s) {
  initt(t);
  startt(t);
  bool ret = (dev_rcq_.poll_once([this](const ibv_wc& wc) {
    recv_posted_--;
    if (wc.opcode != IBV_WC_RECV_RDMA_WITH_IMM)
      mv_serve_recv((packet*)wc.wr_id);
    else
      mv_recv_imm(wc.imm_data);
  }));
  ret |= (dev_scq_.poll_once(
      [](const ibv_wc& wc) { mv_serve_send((packet*)wc.wr_id); }));
  stopt(t);
  // Make sure we always have enough packet, but do not block.
  if (recv_posted_ < MAX_RECV) post_recv(mv_pp_alloc_nb(pkpool, 0));
  // assert(recv_posted_ > 0 && "No posted buffer");
  return ret;
}

void ServerRdmax::serve() {
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

MV_INLINE void ServerRdmax::write_send(int rank, void* buf, size_t size, void* ctx) {
  if (size <= conn[rank].qp().max_inline()) {
    conn[rank].write_send(buf, size, sbuf_.lkey(), 0);
    mv_pp_free_to(pkpool, (packet*) ctx, ((packet*) ctx)->header.poolid);
  } else {
    conn[rank].write_send(buf, size, sbuf_.lkey(), ctx);
  }
}

MV_INLINE void ServerRdmax::write_rma(int rank, void* from, uint32_t lkey, void* to, uint32_t rkey,
                            size_t size, void* ctx) {
  conn[rank].write_rdma(from, lkey, to, rkey, size, ctx);
}

MV_INLINE void ServerRdmax::write_rma_signal(int rank, void* from, uint32_t lkey, void* to, uint32_t rkey,
                            size_t size, uint32_t sid, void* ctx) {
  conn[rank].write_rdma_imm(from, lkey, to, rkey, size, sid, ctx);
}

MV_INLINE void* ServerRdmax::allocate(size_t s) { return heap_segment.allocate(s); }

MV_INLINE void ServerRdmax::deallocate(void* ptr) { heap_segment.deallocate(ptr); }

MV_INLINE void ServerRdmax::finalize() {
  stop_ = true;
  poll_thread_.join();
  dev_scq_.finalize();
  dev_rcq_.finalize();
  sbuf_.finalize();
  heap_.finalize();
}

#endif
