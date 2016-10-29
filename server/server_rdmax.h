#ifndef SERVER_RDMAX_H_
#define SERVER_RDMAX_H_

class server_rdmax : server_base {
 public:
  server_rdmax() : stop_(false), done_init_(false) {}
  inline void init(packet_manager& pkpool, int& rank, int& size);
  inline void post_recv(mpiv_packet* p);
  inline void serve();
  inline void finalize();
  inline void write_send(int rank, void* buf, size_t size, void* ctx);
  inline void write_rma(int rank, void* from, void* to, uint32_t rkey, size_t size, void* ctx);
  inline void* allocate(size_t s);
  inline void deallocate(void* ptr);
  inline uint32_t heap_rkey() { return heap_.rkey(); }

 private:
  inline bool progress();
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
  mbuffer heap_segment;
  vector<connection> conn;
};

void server_rdmax::init(packet_manager& pkpool, int& rank,
    int& size) {
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

  sbuf_alloc_ =
    std::move(std::unique_ptr<pinned_pool>(new pinned_pool(sbuf_.ptr())));
  heap_ = dev_ctx_->create_memory((size_t)HEAP_SIZE, mr_flags);

  heap_segment = std::move(mbuffer(boost::interprocess::create_only,
        heap_.ptr(), (size_t)HEAP_SIZE));

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  for (int i = 0; i < size; i++) {
    conn.emplace_back(&dev_scq_, &dev_rcq_, dev_ctx_.get(), &sbuf_, i);
  }

  // Prepare the packet_mgr and prepost some packet.
  for (int i = 0; i < MAX_SEND; i++) {
    pkpool.ret_packet((mpiv_packet*)sbuf_alloc_->allocate());
  }

  for (int i = 0; i < MAX_RECV; i++) {
    pkpool.ret_packet((mpiv_packet*)sbuf_alloc_->allocate());
  }

  pk_mgr_ptr = &pkpool;
  done_init_ = true;
}

void server_rdmax::post_recv(mpiv_packet* p) {
  if (p == NULL) return;
  recv_posted_++;
  dev_ctx_->post_srq_recv((void*)p, (void*)p, sizeof(mpiv_packet),
      sbuf_.lkey());
}

bool server_rdmax::progress() {  // profiler& p, long long& r, long long &s) {
  initt(t);
  startt(t);
  bool ret = (dev_rcq_.poll_once([this](const ibv_wc& wc) {
        recv_posted_--;
        mpiv_serve_recv((mpiv_packet*) wc.wr_id);
  }));
  ret |= (dev_scq_.poll_once([](const ibv_wc& wc) {
      mpiv_serve_send((mpiv_packet*) wc.wr_id);
  }));
  stopt(t)
    // Make sure we always have enough packet, but do not block.
  if (recv_posted_ < MAX_RECV) post_recv(pk_mgr_ptr->get_for_recv());
  // assert(recv_posted_ > 0 && "No posted buffer");
  return ret;
}

void server_rdmax::serve() {
  poll_thread_ = std::thread([this] {
      __wid = -1;
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

void server_rdmax::write_send(int rank, void* buf, size_t size, void* ctx) {
    conn[rank].write_send(buf, size, sbuf_.lkey(), ctx);
}

void server_rdmax::write_rma(int rank, void* from, void* to, uint32_t rkey, size_t size, void* ctx) {
    conn[rank].write_rdma(from, heap_.lkey(), to, rkey, size, ctx);
}

void* server_rdmax::allocate(size_t s) {
    return heap_segment.allocate(s);
}

void server_rdmax::deallocate(void* ptr) {
    heap_segment.deallocate(ptr);
}

void server_rdmax::finalize() {
  stop_ = true;
  poll_thread_.join();
  dev_scq_.finalize();
  dev_rcq_.finalize();
  sbuf_.finalize();
  heap_.finalize();
}

#endif
