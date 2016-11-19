#ifndef SERVER_RDMAX_H_
#define SERVER_RDMAX_H_

#include "mv.h"
#include "rdmax.h"

using rdmax::device_ctx;
using rdmax::device_cq;
using rdmax::device_memory;
using rdmax::connection;

struct rdmax_server {
  std::thread poll_thread;
  volatile bool stop;
  mv_engine* mv;
  unique_ptr<device_ctx> dev_ctx;
  device_cq dev_scq;
  device_cq dev_rcq;
  device_memory sbuf;
  device_memory heap;
  mbuffer heap_segment;
  int recv_posted;
  unique_ptr<pinned_pool> sbuf_alloc;
  mv_pp* pkpool;
  vector<connection> conn;
} __attribute__((aligned(64)));

inline void rdmax_init(rdmax_server** s, mv_engine* mv, mv_pp*, int& rank, int& size);
inline void rdmax_post_recv(rdmax_server* s, packet* p);
inline bool rdmax_progress(rdmax_server* s);
inline void rdmax_serve(rdmax_server* s);
inline void rdmax_write_send(rdmax_server* s, int rank, void* buf, size_t size, void* ctx);
inline void rdmax_write_rma(rdmax_server* s, int rank, void* from,
    uint32_t lkey, void* to, uint32_t rkey, size_t size, void* ctx);
inline void rdmax_write_rma_signal(rdmax_server *s, int rank, void* from,
    uint32_t lkey, void* to, uint32_t rkey, size_t size, uint32_t sid, void* ctx);
inline void* rdmax_allocate(rdmax_server* s, size_t size);
inline void rdmax_deallocate(rdmax_server* s, void* ptr);
inline void rdmax_finalize(rdmax_server* s);

inline void rdmax_init(rdmax_server** s_ptr, mv_engine* mv, mv_pp* pkpool, int& rank, int& size) {
  rdmax_server* s = new rdmax_server();
  s->stop = true;
#ifdef USE_AFFI
  affinity::set_me_to(0);
#endif
  std::vector<rdmax::device> devs = rdmax::device::get_devices();
  assert(devs.size() > 0 && "Unable to find any ibv device");

  s->dev_ctx =
      std::move(std::unique_ptr<device_ctx>(new device_ctx(devs.back())));
  s->dev_scq = std::move(s->dev_ctx->create_cq(64 * 1024));
  s->dev_rcq = std::move(s->dev_ctx->create_cq(64 * 1024));

  // Create RDMA memory.
  int mr_flags =
      IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;

  // These are pinned memory.
  s->sbuf = s->dev_ctx->create_memory(sizeof(packet) * (MAX_SEND + MAX_RECV + 2),
      mr_flags);

  s->sbuf_alloc =
      std::move(std::unique_ptr<pinned_pool>(new pinned_pool(s->sbuf.ptr())));
  s->heap = s->dev_ctx->create_memory((size_t)HEAP_SIZE, mr_flags);

  s->heap_segment = std::move(mbuffer(boost::interprocess::create_only,
        s->heap.ptr(), (size_t)HEAP_SIZE));

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  for (int i = 0; i < size; i++) {
    s->conn.emplace_back(&s->dev_scq, &s->dev_rcq, s->dev_ctx.get(), &s->heap, i);
  }

  // Prepare the packet_mgr and prepost some packet.
  for (int i = 0; i < MAX_SEND + MAX_RECV; i++) {
    mv_pp_free(pkpool, (packet*)s->sbuf_alloc->allocate());
  }
  s->recv_posted = 0;
  s->mv = mv;
  s->pkpool = pkpool;
  *s_ptr = s;
}

inline void rdmax_post_recv(rdmax_server* s, packet* p) {
  if (p == NULL) return;
  s->recv_posted++;
  s->dev_ctx->post_srq_recv((void*)p, (void*)p, sizeof(packet), s->sbuf.lkey());
}

inline bool rdmax_progress(rdmax_server* s) {  // profiler& p, long long& r, long long &s) {
  initt(t);
  startt(t);
  bool ret = (s->dev_rcq.poll_once([s](const ibv_wc& wc) {
    s->recv_posted--;
    if (wc.opcode != IBV_WC_RECV_RDMA_WITH_IMM)
      mv_serve_recv(s->mv, (packet*)wc.wr_id);
    else
      mv_serve_imm(wc.imm_data);
  }));
  ret |= (s->dev_scq.poll_once(
      [s](const ibv_wc& wc) { mv_serve_send(s->mv, (packet*)wc.wr_id); }));
  stopt(t);
  // Make sure we always have enough packet, but do not block.
  if (s->recv_posted < MAX_RECV) rdmax_post_recv(s, mv_pp_alloc_nb(s->pkpool, 0));
  // assert(recv_posted_ > 0 && "No posted buffer");
  return ret;
}

inline void rdmax_serve(rdmax_server* s) {
  s->stop = false;
  s->poll_thread = std::thread([s] {
#ifdef USE_AFFI
    affinity::set_me_to_last();
#endif

#ifdef USE_PAPI
    profiler server({PAPI_L1_DCM});
    server.start();
#endif

    while (unlikely(!s->stop)) {
      while (rdmax_progress(s)) {
      };
    }

#ifdef USE_PAPI
    server.stop();
    server.print();
#endif
  });
}

inline void rdmax_write_send(rdmax_server* s, int rank, void* buf, size_t size, void* ctx) {
  if (size <= s->conn[rank].qp().max_inline()) {
    s->conn[rank].write_send(buf, size, s->sbuf.lkey(), 0);
    mv_pp_free_to(s->pkpool, (packet*) ctx, ((packet*) ctx)->header.poolid);
  } else {
    s->conn[rank].write_send(buf, size, s->sbuf.lkey(), ctx);
  }
}

inline void rdmax_write_rma(rdmax_server* s, int rank, void* from, void* to, uint32_t rkey,
    size_t size, void* ctx) {
  s->conn[rank].write_rdma(from, s->heap.lkey(), to, rkey, size, ctx);
}

inline void rdmax_write_rma_signal(rdmax_server* s, int rank, void* from, void* to, uint32_t rkey,
    size_t size, uint32_t sid, void* ctx) {
  s->conn[rank].write_rdma_imm(from, s->heap.lkey(), to, rkey, size, sid, ctx);
}

inline void* rdmax_allocate(rdmax_server* s, size_t size) { return s->heap_segment.allocate(size); }

inline void rdmax_deallocate(rdmax_server* s, void* ptr) { s->heap_segment.deallocate(ptr); }

inline void rdmax_finalize(rdmax_server* s) {
  s->stop = true;
  s->poll_thread.join();
  s->dev_scq.finalize();
  s->dev_rcq.finalize();
  s->sbuf.finalize();
  s->heap.finalize();
}

inline uint32_t rdmax_heap_rkey(rdmax_server* s) { return s->heap.rkey(); }

#define mv_server_init rdmax_init
#define mv_server_serve rdmax_serve
#define mv_server_send rdmax_write_send
#define mv_server_rma rdmax_write_rma
#define mv_server_rma_signal rdmax_write_rma_signal
#define mv_server_heap_rkey rdmax_heap_rkey
#define mv_server_alloc rdmax_allocate
#define mv_server_dealloc rdmax_deallocate
#define mv_server_finalize rdmax_finalize

#endif
