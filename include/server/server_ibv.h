#ifndef SERVER_IBV_H
#define SERVER_IBV_H

#include <mpi.h>

#include "mv.h"
#include "pool.h"
#include "infiniband/verbs.h"

#include "profiler.h"

#define ALIGNMENT (4096)

// #define IBV_SERVER_DEBUG

#ifdef IBV_SERVER_DEBUG
#define IBV_SAFECALL(x)                                                    \
  {                                                                       \
    int err = (x);                                                        \
    if (err) {                                                            \
      printf("err : %d (%s:%d)\n", err, __FILE__, __LINE__); \
      MPI_Abort(MPI_COMM_WORLD, err);                                     \
    }                                                                     \
  }
#else
#define IBV_SAFECALL(x) { (x); }
#endif

struct conn_ctx {
  uint64_t addr;
  uint32_t rkey;
  uint32_t qp_num;
  uint16_t lid;
  ibv_gid gid;
};

typedef ibv_mr mv_server_memory;

struct ibv_server {
  // MV fields.
  mv_engine* mv;

  // Polling threads.
  std::thread poll_thread;

  // Device fields.
  ibv_context* dev_ctx;
  ibv_pd* dev_pd;
  ibv_srq* dev_srq;
  ibv_cq* send_cq;
  ibv_cq* recv_cq;
  mv_server_memory* sbuf;
  mv_server_memory* heap;

  // Connections O(N)
  ibv_qp** dev_qp;
  conn_ctx* conn;

  // Helper fields.
  uint32_t max_inline;
  mv_pool* sbuf_pool;
  void* heap_ptr;
  int recv_posted;
} __attribute__((aligned(64)));


inline void ibv_server_init(mv_engine* mv, size_t heap_size, ibv_server** s_ptr);
inline void ibv_server_post_recv(ibv_server* s, packet* p);
inline void ibv_server_serve(ibv_server* s);
inline void ibv_server_write_send(ibv_server* s, int rank, void* buf, size_t size,
                             void* ctx);
inline void ibv_server_write_rma(ibv_server* s, int rank, void* from,
                            uint32_t lkey, void* to, uint32_t rkey, size_t size,
                            void* ctx);
inline void ibv_server_write_rma_signal(ibv_server* s, int rank, void* from,
                                   uint32_t lkey, void* to, uint32_t rkey,
                                   size_t size, uint32_t sid, void* ctx);
inline void ibv_server_finalize(ibv_server* s);


inline mv_server_memory* ibv_server_mem_malloc(ibv_server* s, size_t size) {
  int mr_flags =
      IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;
  void* ptr = 0;
  posix_memalign(&ptr, 4096, size + 4096); 
  return ibv_reg_mr(s->dev_pd, ptr, size, mr_flags);
}

inline void ibv_server_mem_free(mv_server_memory* mr) {
  ibv_dereg_mr(mr);
}

static ibv_qp* qp_create(ibv_server* s, ibv_device_attr* dev_attr) {
  ibv_qp_init_attr qp_init_attr;
  qp_init_attr.qp_context = 0;
  qp_init_attr.send_cq = s->send_cq;
  qp_init_attr.recv_cq = s->recv_cq;
  qp_init_attr.srq = s->dev_srq;
  qp_init_attr.cap = 
      {
        (uint32_t)dev_attr->max_qp_wr, // max_send_wr
        (uint32_t)dev_attr->max_qp_wr, // max_recv_wr
        // -- this affect the size of inline (TODO:tune later).
        1, // max_send_sge -- some device only allows 1..
        1, // max_recv_sge
        0, // max_inline_data;  -- this do nothing.
      };
  qp_init_attr.qp_type = IBV_QPT_RC; // ibv_qp_type qp_type;
  qp_init_attr.sq_sig_all = 0;       // int sq_sig_all;
  ibv_qp* qp = ibv_create_qp(s->dev_pd, &qp_init_attr);
  s->max_inline = std::min(s->max_inline, (uint32_t) qp_init_attr.cap.max_inline_data);
  return qp;
}

static void qp_init(ibv_qp* qp, int port) {
  ibv_qp_attr attr;
  memset(&attr, 0, sizeof(ibv_qp_attr));
  attr.qp_state = IBV_QPS_INIT;
  attr.port_num = port;
  attr.pkey_index = 0;
  attr.qp_access_flags =
      IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;
  int flags =
      IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS;
  int rc = ibv_modify_qp(qp, &attr, flags);
  if (rc != 0) {
    printf("Unable to init qp\n");
    exit(EXIT_FAILURE);
  }
}

static void qp_to_rtr(ibv_qp* qp, int dev_port, ibv_port_attr* port_attr, conn_ctx* rmtctx_) {
  ibv_qp_attr attr;
  memset(&attr, 0, sizeof(ibv_qp_attr));

  attr.qp_state = IBV_QPS_RTR;
  attr.path_mtu = port_attr->active_mtu;
  attr.dest_qp_num = rmtctx_->qp_num;
  attr.rq_psn = 0;
  attr.max_dest_rd_atomic = 1;
  attr.min_rnr_timer = 12;

  attr.ah_attr.is_global = 0;
  attr.ah_attr.dlid = rmtctx_->lid;
  attr.ah_attr.sl = 0;
  attr.ah_attr.src_path_bits = 0;
  attr.ah_attr.port_num = dev_port;

  memcpy(&attr.ah_attr.grh.dgid, &rmtctx_->gid, 16);
  attr.ah_attr.grh.sgid_index = 0;  // gid

  int flags = IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
              IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER;

  int rc = ibv_modify_qp(qp, &attr, flags);
  if (rc != 0) {
    printf("failed to modify QP state to RTR\n");
    exit(EXIT_FAILURE);
  }
}

static void qp_to_rts(ibv_qp* qp) {
  ibv_qp_attr attr;
  memset(&attr, 0, sizeof(ibv_qp_attr));

  attr.qp_state = IBV_QPS_RTS;
  attr.timeout = 0x12;
  attr.retry_cnt = 7;
  attr.rnr_retry = 7;
  attr.sq_psn = 0;
  attr.max_rd_atomic = 1;

  int flags = IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |
              IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC;
  int rc = ibv_modify_qp(qp, &attr, flags);
  if (rc != 0) {
    printf("failed to modify QP state to RTS\n");
    exit(EXIT_FAILURE);
  }
}

inline void ibv_server_init(mv_engine* mv, size_t heap_size,
                       ibv_server** s_ptr)
{
  ibv_server* s = new ibv_server();
  s->max_inline = 128;

#ifdef USE_AFFI
  affinity::set_me_to(0);
#endif
  int num_devices;
  ibv_device** dev_list = ibv_get_device_list(&num_devices);
  if (num_devices <= 0) {
    printf("Unable to find any ibv devices\n");
    exit(EXIT_FAILURE);
  }
  // Use the last one by default.
  s->dev_ctx = ibv_open_device(dev_list[num_devices - 1]);
  ibv_device_attr dev_attr;
  ibv_query_device(s->dev_ctx, &dev_attr);
  int rc = 0;
  ibv_port_attr port_attr;
  int dev_port = 0;
  for (; dev_port < 128; dev_port++) {
    rc = ibv_query_port(s->dev_ctx, dev_port, &port_attr);
    if (rc == 0) break;
  }
  if (rc != 0) {
    printf("Unable to query port\n");
    exit(EXIT_FAILURE);
  }

  s->dev_pd = ibv_alloc_pd(s->dev_ctx);
  if (s->dev_pd == 0) {
    printf("Could not create protection domain for context\n");
    exit(EXIT_FAILURE);
  }

  // Create shared-receive queue, **number here affect performance**.
  ibv_srq_init_attr srq_attr;
  srq_attr.srq_context = 0;
  srq_attr.attr.max_wr = MAX_RECV;
  srq_attr.attr.max_sge = 1;
  srq_attr.attr.srq_limit = 0;
  s->dev_srq = ibv_create_srq(s->dev_pd, &srq_attr);
  if (s->dev_srq == 0) {
    printf("Could not create shared received queue\n");
    exit(EXIT_FAILURE);
  }
    
  // Create completion queues.
  s->send_cq = ibv_create_cq(s->dev_ctx, 64 * 1024, 0, 0, 0);
  s->recv_cq = ibv_create_cq(s->dev_ctx, 64 * 1024, 0, 0, 0);
  if (s->send_cq == 0 || s->recv_cq == 0) {
    printf("Unable to create cq\n");
    exit(EXIT_FAILURE);
  }

  // Create RDMA memory.
  s->heap = ibv_server_mem_malloc(s, heap_size);
  s->heap_ptr = (void*) s->heap->addr;
  if (s->heap == 0) {
    printf("Unable to create sbuf\n");
    exit(EXIT_FAILURE);
  }

  MPI_Comm_rank(MPI_COMM_WORLD, &mv->me);
  MPI_Comm_size(MPI_COMM_WORLD, &mv->size);
  s->conn = new conn_ctx[mv->size];
  s->dev_qp = new ibv_qp*[mv->size];

  conn_ctx lctx;
  conn_ctx* rmtctx;

  for (int i = 0; i < mv->size; i++) {
    s->dev_qp[i] = qp_create(s, &dev_attr);
    if (!s->dev_qp[i]) {
      printf("Unable to create queue pair\n");
      exit(EXIT_FAILURE);
    }

    rmtctx = &s->conn[i];
    lctx.addr = (uintptr_t) s->heap_ptr;
    lctx.rkey = s->heap->rkey;
    lctx.qp_num = s->dev_qp[i]->qp_num;
    lctx.lid = port_attr.lid;
    rc = ibv_query_gid(s->dev_ctx, dev_port, 0, &lctx.gid);
    if (rc != 0) {
      printf("Unable to query gid\n");
      exit(EXIT_FAILURE);
    }

    // Exchange connection conn_ctx.
    MPI_Sendrecv(&lctx, sizeof(conn_ctx), MPI_BYTE, i, 0, rmtctx,
        sizeof(conn_ctx), MPI_BYTE, i, 0, MPI_COMM_WORLD,
        MPI_STATUS_IGNORE);

    qp_init(s->dev_qp[i], dev_port);
    qp_to_rtr(s->dev_qp[i], dev_port, &port_attr, rmtctx);
    qp_to_rts(s->dev_qp[i]);
  }

  // Prepare the packet_mgr and prepost some packet.
  s->sbuf = ibv_server_mem_malloc(s, sizeof(packet) * (MAX_SEND + MAX_RECV));
  mv_pool_create(&s->sbuf_pool, (void*) s->sbuf->addr, sizeof(packet), MAX_SEND + MAX_RECV);
#if 0
  for (int i = 0; i < MAX_SEND + MAX_RECV; i++) {
    mv_pp_free(pkpool, (packet*) mv_pool_get(s->sbuf_pool));
  }
#endif

  s->recv_posted = 0;
  s->mv = mv;
  s->mv->pkpool = s->sbuf_pool;
  *s_ptr = s;
}

inline void ibv_server_post_recv(ibv_server* s, packet* p)
{
  if (p == NULL) return;
  s->recv_posted++;

  ibv_sge sg = {
    (uintptr_t) p, // addr
    sizeof(packet), //length
    s->sbuf->lkey,
  };

  ibv_recv_wr wr = {
    (uintptr_t) p, // wr_id
    0, // next
    &sg, // sg_list
    1, // num_sge
  };

  ibv_recv_wr* bad_wr = 0;
  if (ibv_post_srq_recv(s->dev_srq, &wr, &bad_wr)) {
    printf("Unable to post_srq\n");
    exit(EXIT_FAILURE);
  }
}

MV_INLINE bool ibv_server_progress(ibv_server* s)
{  // profiler& p, long long& r, long long &s) {
  initt(t);
  startt(t);
  bool ret = false;
  ibv_wc wc;
  int ne = ibv_poll_cq(s->recv_cq, 1, &wc);
  if (ne == 1) {
    s->recv_posted--;
    if (wc.opcode != IBV_WC_RECV_RDMA_WITH_IMM)
      mv_serve_recv(s->mv, (packet*)wc.wr_id);
    else
      mv_serve_imm(wc.imm_data);
    ret = true;
  }
  ne = ibv_poll_cq(s->send_cq, 1, &wc);
  if (ne == 1) {
    mv_serve_send(s->mv, (packet*)wc.wr_id);
    ret = true;
  }
  stopt(t);
  // Make sure we always have enough packet, but do not block.
  if (s->recv_posted < MAX_RECV)
    ibv_server_post_recv(s, (packet*) mv_pool_get_nb(s->sbuf_pool)); //, 0));

  return ret;
}

static MV_INLINE void setup_wr(ibv_send_wr& wr, uintptr_t id, ibv_sge* l,
                            ibv_wr_opcode mode, int flags)
{
  wr.wr_id = id;
  wr.sg_list = l;
  wr.num_sge = 1;
  wr.opcode = mode;
  wr.send_flags = flags;
  wr.next = NULL;
}

inline void ibv_server_write_send(ibv_server* s, int rank, void* buf, size_t size,
                             void* ctx)
{
  ibv_sge list = {
    (uintptr_t)buf, // address
    (uint32_t)size, // length
    s->sbuf->lkey,           // lkey
  };

  ibv_send_wr this_wr;
  ibv_send_wr* bad_wr;

  if (size <= s->max_inline) {
    setup_wr(this_wr, 0, &list, IBV_WR_SEND_WITH_IMM, IBV_SEND_SIGNALED);
    this_wr.send_flags |= IBV_SEND_INLINE;
    this_wr.imm_data = 0;
    IBV_SAFECALL(ibv_post_send(s->dev_qp[rank], &this_wr, &bad_wr));
    // Must be in the same threads.
    mv_pool_put(s->sbuf_pool, ctx); 
  } else {
    setup_wr(this_wr, (uintptr_t) ctx, &list, IBV_WR_SEND_WITH_IMM, IBV_SEND_SIGNALED);
    this_wr.imm_data = 0;
    IBV_SAFECALL(ibv_post_send(s->dev_qp[rank], &this_wr, &bad_wr));
  }
}

inline void ibv_server_write_rma(ibv_server* s, int rank, void* from, void* to,
                            uint32_t rkey, size_t size, void* ctx)
{
  ibv_send_wr this_wr;// = {0};

  ibv_sge list = {
      (uintptr_t)from,  // address
      (unsigned)size,       // length
      s->heap->lkey, // lkey
  };

  int flags = (size <= s->max_inline ? IBV_SEND_INLINE : 0) | IBV_SEND_SIGNALED;
  setup_wr(this_wr, (uintptr_t)ctx, &list, IBV_WR_RDMA_WRITE, flags);

  this_wr.next = NULL;
  ibv_send_wr* bad_wr;
  this_wr.wr.rdma.remote_addr = (uintptr_t)to;
  this_wr.wr.rdma.rkey = rkey;

  IBV_SAFECALL(ibv_post_send(s->dev_qp[rank], &this_wr, &bad_wr));
}

inline void ibv_server_write_rma_signal(ibv_server* s, int rank, void* from,
                                   void* to, uint32_t rkey, size_t size,
                                   uint32_t sid, void* ctx)
{
  ibv_send_wr this_wr;// = {0};

  ibv_sge list = {
      (uintptr_t)from,  // address
      (unsigned)size,       // length
      s->heap->lkey, // lkey
  };

  int flags = (size <= s->max_inline ? IBV_SEND_INLINE : 0) | IBV_SEND_SIGNALED;
  setup_wr(this_wr, (uintptr_t)ctx, &list, IBV_WR_RDMA_WRITE_WITH_IMM, flags);

  this_wr.next = NULL;
  ibv_send_wr* bad_wr;

  this_wr.wr.rdma.remote_addr = (uintptr_t)to;
  this_wr.wr.rdma.rkey = rkey;
  this_wr.imm_data = sid;

  IBV_SAFECALL(ibv_post_send(s->dev_qp[rank], &this_wr, &bad_wr));
}

inline void ibv_server_finalize(ibv_server* s)
{
  // TODO(danghvu):
  // s->dev_scq.finalize();
  // s->dev_rcq.finalize();
  // s->sbuf.finalize();
  // s->heap.finalize();
}

inline uint32_t ibv_server_heap_rkey(ibv_server* s) { return s->heap->rkey; }
inline uint32_t ibv_server_heap_rkey(ibv_server* s, int node)
{
  return s->conn[node].rkey;
}

inline void* ibv_server_heap_ptr(ibv_server* s) { return s->heap_ptr; }

#define mv_server_init ibv_server_init
#define mv_server_send ibv_server_write_send
#define mv_server_rma ibv_server_write_rma
#define mv_server_rma_signal ibv_server_write_rma_signal
#define mv_server_heap_rkey ibv_server_heap_rkey
#define mv_server_heap_ptr ibv_server_heap_ptr
#define mv_server_progress ibv_server_progress
#define mv_server_finalize ibv_server_finalize

#endif
