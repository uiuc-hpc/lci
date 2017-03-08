#ifndef SERVER_IBV_H_
#define SERVER_IBV_H_

#include <mpi.h>

#include "mv.h"
#include "mv/affinity.h"

#include "dreg/dreg.h"

#include "infiniband/verbs.h"

#include "mv/profiler.h"

#define ALIGNMENT (4096)
#define MAX_CQ 16

// #define IBV_SERVER_DEBUG

#ifdef IBV_SERVER_DEBUG
#define IBV_SAFECALL(x)                                      \
  {                                                          \
    int err = (x);                                           \
    if (err) {                                               \
      printf("err : %d (%s:%d)\n", err, __FILE__, __LINE__); \
      MPI_Abort(MPI_COMM_WORLD, err);                        \
    }                                                        \
  }                                                          \
  while (0)                                                  \
    ;
#else
#define IBV_SAFECALL(x) \
  {                     \
    x;                  \
  }                     \
  while (0)             \
    ;
#endif

struct conn_ctx {
  uint64_t addr;
  uint32_t rkey;
  uint32_t qp_num;
  uint16_t lid;
  union ibv_gid gid;
};

typedef struct ibv_mr mv_server_memory;

typedef struct ibv_server {
  // MV fields.
  mvh* mv;

  // Device fields.
  struct ibv_context* dev_ctx;
  struct ibv_pd* dev_pd;
  struct ibv_srq* dev_srq;
  struct ibv_cq* send_cq;
  struct ibv_cq* recv_cq;
  mv_server_memory* sbuf;
  mv_server_memory* heap;

  // Connections O(N)
  struct ibv_qp** dev_qp;
  struct conn_ctx* conn;

  // Helper fields.
  void* heap_ptr;
  long recv_posted;
} ibv_server __attribute__((aligned(64)));

extern size_t server_max_inline;

MV_INLINE void ibv_server_init(mvh* mv, size_t heap_size, ibv_server** s_ptr);
MV_INLINE void ibv_server_finalize(ibv_server* s);
MV_INLINE mv_server_memory* ibv_server_mem_malloc(ibv_server* s, size_t size);
MV_INLINE void ibv_server_mem_free(mv_server_memory* mr);

MV_INLINE void ibv_server_post_recv(ibv_server* s, mv_packet* p);
MV_INLINE int ibv_server_write_send(ibv_server* s, int rank, void* buf,
                                    size_t size, void* ctx);
MV_INLINE void ibv_server_write_rma(ibv_server* s, int rank, void* from,
                                    uintptr_t to, uint32_t rkey, size_t size,
                                    void* ctx);
MV_INLINE void ibv_server_write_rma_signal(ibv_server* s, int rank, void* from,
                                           uintptr_t addr, uint32_t rkey,
                                           size_t size, uint32_t sid,
                                           void* ctx);
MV_INLINE int ibv_server_progress(ibv_server* s);

MV_INLINE void* ibv_server_heap_ptr(mv_server* s);
MV_INLINE uint32_t ibv_server_heap_rkey(mv_server* s, int node);

MV_INLINE struct ibv_qp* qp_create(ibv_server* s,
                                   struct ibv_device_attr* dev_attr)
{
  struct ibv_qp_init_attr qp_init_attr;
  qp_init_attr.qp_context = 0;
  qp_init_attr.send_cq = s->send_cq;
  qp_init_attr.recv_cq = s->recv_cq;
  qp_init_attr.srq = s->dev_srq;
  qp_init_attr.cap.max_send_wr = (uint32_t)dev_attr->max_qp_wr;
  qp_init_attr.cap.max_recv_wr = (uint32_t)dev_attr->max_qp_wr;
  // -- this affect the size of (TODO:tune later).
  qp_init_attr.cap.max_send_sge = 1;
  qp_init_attr.cap.max_recv_sge = 1;
  qp_init_attr.cap.max_inline_data = 0;
  qp_init_attr.qp_type = IBV_QPT_RC;
  qp_init_attr.sq_sig_all = 0;
  struct ibv_qp* qp = ibv_create_qp(s->dev_pd, &qp_init_attr);
  server_max_inline = MIN(qp_init_attr.cap.max_inline_data, SERVER_MAX_INLINE);
  return qp;
}

MV_INLINE void qp_init(struct ibv_qp* qp, int port)
{
  struct ibv_qp_attr attr;
  memset(&attr, 0, sizeof(struct ibv_qp_attr));
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

MV_INLINE void qp_to_rtr(struct ibv_qp* qp, int dev_port,
                         struct ibv_port_attr* port_attr,
                         struct conn_ctx* rmtctx_)
{
  struct ibv_qp_attr attr;
  memset(&attr, 0, sizeof(struct ibv_qp_attr));

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

MV_INLINE void qp_to_rts(struct ibv_qp* qp)
{
  struct ibv_qp_attr attr;
  memset(&attr, 0, sizeof(struct ibv_qp_attr));

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

MV_INLINE uintptr_t _real_ibv_reg(ibv_server* s, void* buf, size_t size)
{
  int mr_flags =
      IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;
  return (uintptr_t)ibv_reg_mr(s->dev_pd, buf, size, mr_flags);
}

MV_INLINE uintptr_t ibv_rma_reg(ibv_server* s, void* buf, size_t size)
{
  return (uintptr_t)dreg_register(s, buf, size);
}

MV_INLINE int ibv_rma_dereg(uintptr_t mem)
{
  dreg_unregister((dreg_entry*)mem);
  return 1;
}

MV_INLINE uint32_t ibv_rma_key(uintptr_t mem)
{
  // return ((struct ibv_mr*) mem)->rkey;
  return ((struct ibv_mr*)(((dreg_entry*)mem)->memhandle[0]))->rkey;
}

MV_INLINE uint32_t ibv_rma_lkey(uintptr_t mem)
{
  return ((struct ibv_mr*)(((dreg_entry*)mem)->memhandle[0]))->lkey;
  // return ((struct ibv_mr*) mem)->lkey;
}

MV_INLINE void ibv_server_post_recv(ibv_server* s, mv_packet* p)
{
  if (p == NULL) return;
  s->recv_posted++;

  struct ibv_sge sg = {
      .addr = (uintptr_t)(&p->data),
      .length = POST_MSG_SIZE,
      .lkey = s->heap->lkey,
  };

  struct ibv_recv_wr wr = {
      .wr_id = (uintptr_t) & (p->context),
      .next = 0,
      .sg_list = &sg,
      .num_sge = 1,
  };

  struct ibv_recv_wr* bad_wr = 0;
  IBV_SAFECALL(ibv_post_srq_recv(s->dev_srq, &wr, &bad_wr));
}

MV_INLINE int ibv_progress_recv_once(ibv_server* s)
{
  struct ibv_wc wc;
  int ret = ibv_poll_cq(s->recv_cq, 1, &wc);
  if (ret > 0) {
    return 1;
  }
  return 0;
}

MV_INLINE int ibv_progress_send_once(ibv_server* s)
{
  struct ibv_wc wc;
  int ret = ibv_poll_cq(s->send_cq, 1, &wc);
  if (ret > 0) {
    return 1;
  }
  return 0;
}

MV_INLINE int ibv_server_progress(ibv_server* s)
{  // profiler& p, long long& r, long long &s) {
  struct ibv_wc wc[MAX_CQ];
  int ret = 0;
  int ne = ibv_poll_cq(s->recv_cq, MAX_CQ, wc);

  if (ne > 0) {
    for (int i = 0; i < ne; i++) {
#ifdef IBV_SERVER_DEBUG
      if (wc[i].status != IBV_WC_SUCCESS) {
        fprintf(stderr, "Failed status %s (%d) for wr_id %d\n",
                ibv_wc_status_str(wc[i].status), wc[i].status,
                (int)wc[i].wr_id);
        exit(EXIT_FAILURE);
      }
#endif
      s->recv_posted--;
      if (wc[i].opcode != IBV_WC_RECV_RDMA_WITH_IMM)
        mv_serve_recv(s->mv, (mv_packet*)wc[i].wr_id);
      else {
        mv_serve_imm(s->mv, wc[i].imm_data);
        mv_pool_put(s->mv->pkpool, (mv_packet*)wc[i].wr_id);
      }
    }
    ret = 1;
  }

  ne = ibv_poll_cq(s->send_cq, MAX_CQ, wc);

  if (ne > 0) {
    for (int i = 0; i < ne; i++) {
#ifdef IBV_SERVER_DEBUG
      if (wc[i].status != IBV_WC_SUCCESS) {
        fprintf(stderr, "Failed status %s (%d) for wr_id %d\n",
                ibv_wc_status_str(wc[i].status), wc[i].status,
                (int)wc[i].wr_id);
        exit(EXIT_FAILURE);
      }
#endif
      mv_serve_send(s->mv, (mv_packet*)wc[i].wr_id);
    }
    ret = 1;
  }

  // Make sure we always have enough packet, but do not block.
  if (s->recv_posted < MAX_RECV) {
    ibv_server_post_recv(s,
                         (mv_packet*)mv_pool_get_nb(s->mv->pkpool));  //, 0));
  }

#ifdef IBV_SERVER_DEBUG
  if (s->recv_posted == 0) {
    printf("WARNING DEADLOCK %d\n", s->recv_posted);
  }
#endif

  return ret;
}

#define setup_wr(w, d, l, m, f) \
  {                             \
    (w).wr_id = (d);            \
    (w).sg_list = (l);          \
    (w).num_sge = (1);          \
    (w).opcode = (m);           \
    (w).send_flags = (f);       \
    (w).next = NULL;            \
  }                             \
  while (0)                     \
    ;

static uint8_t count_inline = 0;

/*! This return whether or not to wait. */
MV_INLINE int ibv_server_write_send(ibv_server* s, int rank, void* buf,
                                    size_t size, void* ctx)
{
  struct ibv_sge list = {
      .addr = (uintptr_t)buf,    // address
      .length = (uint32_t)size,  // length
      .lkey = s->heap->lkey,     // lkey
  };

  struct ibv_send_wr this_wr;
  struct ibv_send_wr* bad_wr;

  if (size <= server_max_inline) {
    setup_wr(this_wr, (uintptr_t)0, &list, IBV_WR_SEND,
             IBV_SEND_INLINE | IBV_SEND_SIGNALED);
    IBV_SAFECALL(ibv_post_send(s->dev_qp[rank], &this_wr, &bad_wr));
    mv_serve_send(s->mv, ctx);
    // count_inline ++;
    return 0;
  } else {
    count_inline = 0;
    setup_wr(this_wr, (uintptr_t)ctx, &list, IBV_WR_SEND, IBV_SEND_SIGNALED);
    IBV_SAFECALL(ibv_post_send(s->dev_qp[rank], &this_wr, &bad_wr));
    return 1;
  }
}

MV_INLINE void ibv_server_write_rma(ibv_server* s, int rank, void* from,
                                    uintptr_t to, uint32_t rkey, size_t size,
                                    void* ctx)

{
  struct ibv_send_wr this_wr;
  struct ibv_send_wr* bad_wr = 0;

  struct ibv_sge list = {
      .addr = (uintptr_t)from,   // address
      .length = (unsigned)size,  // length
      .lkey = s->heap->lkey,     // lkey
  };

  int flags =
      (size <= SERVER_MAX_INLINE ? IBV_SEND_INLINE : 0) | IBV_SEND_SIGNALED;
  setup_wr(this_wr, (uintptr_t)ctx, &list, IBV_WR_RDMA_WRITE, flags);
  this_wr.wr.rdma.remote_addr = (uintptr_t)to;
  this_wr.wr.rdma.rkey = rkey;

  IBV_SAFECALL(ibv_post_send(s->dev_qp[rank], &this_wr, &bad_wr));
}

MV_INLINE void ibv_server_write_rma_signal(ibv_server* s, int rank, void* from,
                                           uintptr_t addr, uint32_t rkey,
                                           size_t size, uint32_t sid, void* ctx)
{
  struct ibv_send_wr this_wr;  // = {0};
  struct ibv_send_wr* bad_wr = 0;

  uintptr_t mr = ibv_rma_reg(s, from, size);

  struct ibv_sge list = {
      .addr = (uintptr_t)from,   // address
      .length = (unsigned)size,  // length
      .lkey = ibv_rma_lkey(mr),
  };

  int flags =
      (size <= SERVER_MAX_INLINE ? IBV_SEND_INLINE : 0) | IBV_SEND_SIGNALED;
  setup_wr(this_wr, (uintptr_t)ctx, &list, IBV_WR_RDMA_WRITE_WITH_IMM, flags);

  this_wr.wr.rdma.remote_addr = addr;
  this_wr.wr.rdma.rkey = rkey;
  this_wr.imm_data = sid;

  IBV_SAFECALL(ibv_post_send(s->dev_qp[rank], &this_wr, &bad_wr));
}

MV_INLINE void ibv_server_init(mvh* mv, size_t heap_size, ibv_server** s_ptr)
{
  ibv_server* s = 0;
  posix_memalign((void**)&s, 64, sizeof(ibv_server));
  assert(s);

  int num_devices;
  struct ibv_device** dev_list = ibv_get_device_list(&num_devices);
  if (num_devices <= 0) {
    printf("Unable to find any ibv devices\n");
    exit(EXIT_FAILURE);
  }
  // Use the last one by default.
  s->dev_ctx = ibv_open_device(dev_list[num_devices - 1]);
  if (s->dev_ctx == 0) {
    printf("Unable to find any ibv devices\n");
    exit(EXIT_FAILURE);
  }
  ibv_free_device_list(dev_list);

  struct ibv_device_attr dev_attr;
  int rc = ibv_query_device(s->dev_ctx, &dev_attr);
  if (rc != 0) {
    printf("Unable to query device\n");
    exit(EXIT_FAILURE);
  }

  struct ibv_port_attr port_attr;
  uint8_t dev_port = 0;
  // int (*func)(struct ibv_context*, uint8_t, struct ibv_port_attr *);
  // func = ibv_query_port;
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
  struct ibv_srq_init_attr srq_attr;
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
  s->heap_ptr = (void*)s->heap->addr;

  if (s->heap == 0) {
    printf("Unable to create heap\n");
    exit(EXIT_FAILURE);
  }

  MPI_Comm_rank(MPI_COMM_WORLD, &mv->me);
  MPI_Comm_size(MPI_COMM_WORLD, &mv->size);
  s->conn = (struct conn_ctx*)malloc(sizeof(struct conn_ctx) * mv->size);
  s->dev_qp = (struct ibv_qp**)malloc(sizeof(struct ibv_qp*) * mv->size);

  struct conn_ctx lctx;
  struct conn_ctx* rmtctx;

  for (int i = 0; i < mv->size; i++) {
    s->dev_qp[i] = qp_create(s, &dev_attr);
    if (!s->dev_qp[i]) {
      printf("Unable to create queue pair\n");
      exit(EXIT_FAILURE);
    }

    rmtctx = &s->conn[i];
    lctx.addr = (uintptr_t)s->heap_ptr;
    lctx.rkey = s->heap->rkey;
    lctx.qp_num = s->dev_qp[i]->qp_num;
    lctx.lid = port_attr.lid;
    rc = ibv_query_gid(s->dev_ctx, dev_port, 0, &lctx.gid);
    if (rc != 0) {
      printf("Unable to query gid\n");
      exit(EXIT_FAILURE);
    }

    // Exchange connection conn_ctx.
    MPI_Sendrecv(&lctx, sizeof(struct conn_ctx), MPI_BYTE, i, 0, rmtctx,
                 sizeof(struct conn_ctx), MPI_BYTE, i, 0, MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);

    qp_init(s->dev_qp[i], dev_port);
    qp_to_rtr(s->dev_qp[i], dev_port, &port_attr, rmtctx);
    qp_to_rts(s->dev_qp[i]);
  }

  dreg_init();

  s->recv_posted = 0;
  s->mv = mv;
  *s_ptr = s;
}

MV_INLINE void ibv_server_finalize(ibv_server* s)
{
  ibv_destroy_cq(s->send_cq);
  ibv_destroy_cq(s->recv_cq);
  ibv_destroy_srq(s->dev_srq);
  ibv_server_mem_free(s->heap);
  for (int i = 0; i < s->mv->size; i++) {
    ibv_destroy_qp(s->dev_qp[i]);
  }
  free(s->conn);
  free(s->dev_qp);
  free(s);
}

MV_INLINE mv_server_memory* ibv_server_mem_malloc(ibv_server* s, size_t size)
{
  int mr_flags =
      IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;
  void* ptr = 0;
  posix_memalign(&ptr, 4096, size + 4096);
  return ibv_reg_mr(s->dev_pd, ptr, size, mr_flags);
}

MV_INLINE void ibv_server_mem_free(mv_server_memory* mr)
{
  void* ptr = (void*)mr->addr;
  ibv_dereg_mr(mr);
  free(ptr);
}

MV_INLINE uint32_t ibv_server_heap_rkey(ibv_server* s, int node)
{
  return s->conn[node].rkey;
}

MV_INLINE void* ibv_server_heap_ptr(ibv_server* s) { return s->heap_ptr; }
#define mv_server_init ibv_server_init
#define mv_server_send ibv_server_write_send
#define mv_server_rma ibv_server_write_rma
#define mv_server_rma_signal ibv_server_write_rma_signal
#define mv_server_heap_rkey ibv_server_heap_rkey
#define mv_server_heap_ptr ibv_server_heap_ptr
#define mv_server_progress ibv_server_progress
#define mv_server_finalize ibv_server_finalize
#define mv_server_post_recv ibv_server_post_recv
#define mv_server_progress_send_once ibv_progress_send_once
#define mv_server_progress_recv_once ibv_progress_recv_once

#define mv_server_rma_reg ibv_rma_reg
#define mv_server_rma_key ibv_rma_key
#define mv_server_rma_dereg ibv_rma_dereg

#endif
