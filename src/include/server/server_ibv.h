#ifndef SERVER_IBV_H_
#define SERVER_IBV_H_

#include "pmi.h"

#include "lc/affinity.h"
#include "lc/profiler.h"

#include "dreg/dreg.h"
#include "infiniband/verbs.h"

#define ALIGNMENT (4096)
#define MAX_CQ 16
#define GET_PROTO(p) (p & 0x00ff)

#ifdef LC_SERVER_DEBUG
#define IBV_SAFECALL(x)                                               \
  {                                                                   \
    int err = (x);                                                    \
    if (err) {                                                        \
      fprintf(stderr, "err : %d (%s:%d)\n", err, __FILE__, __LINE__); \
      MPI_Abort(MPI_COMM_WORLD, err);                                 \
    }                                                                 \
  }                                                                   \
  while (0)                                                           \
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

typedef struct ibv_mr lc_server_memory;

typedef struct ibv_server {
  // MV fields.
  lch* mv;

  // Device fields.
  struct ibv_context* dev_ctx;
  struct ibv_pd* dev_pd;
  struct ibv_srq* dev_srq;
  struct ibv_cq* send_cq;
  struct ibv_cq* recv_cq;
  lc_server_memory* sbuf;
  lc_server_memory* heap;

  // Connections O(N)
  struct ibv_qp** dev_qp;
  struct conn_ctx* conn;

  // Helper fields.
  int* qp2rank;
  int qp2rank_mod;
  void* heap_ptr;
  long recv_posted;
  int with_mpi;
} ibv_server __attribute__((aligned(64)));

extern size_t server_max_inline;

LC_INLINE void ibv_server_init(lch* mv, size_t heap_size, ibv_server** s_ptr);
LC_INLINE void ibv_server_finalize(ibv_server* s);
LC_INLINE lc_server_memory* ibv_server_mem_malloc(ibv_server* s, size_t size);
LC_INLINE void ibv_server_mem_free(lc_server_memory* mr);

LC_INLINE void ibv_server_post_recv(ibv_server* s, lc_packet* p);
LC_INLINE int ibv_server_write_send(ibv_server* s, int rank, void* buf,
                                    size_t size, lc_packet* ctx,
                                    uint32_t proto);
LC_INLINE void ibv_server_write_rma(ibv_server* s, int rank, void* from,
                                    uintptr_t to, uint32_t rkey, size_t size,
                                    lc_packet* ctx, uint32_t proto);
LC_INLINE void ibv_server_write_rma_signal(ibv_server* s, int rank, void* from,
                                           uintptr_t addr, uint32_t rkey,
                                           size_t size, uint32_t sid,
                                           lc_packet* ctx);
LC_INLINE int ibv_server_progress(ibv_server* s);

LC_INLINE void* ibv_server_heap_ptr(lc_server* s);
LC_INLINE uint32_t ibv_server_heap_rkey(lc_server* s, int node);

LC_INLINE struct ibv_qp* qp_create(ibv_server* s,
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
  qp_init_attr.cap.max_send_sge = 8;
  qp_init_attr.cap.max_recv_sge = 1;
  qp_init_attr.cap.max_inline_data = 0;
  qp_init_attr.qp_type = IBV_QPT_RC;
  qp_init_attr.sq_sig_all = 0;
  struct ibv_qp* qp = ibv_create_qp(s->dev_pd, &qp_init_attr);
  server_max_inline = MIN(qp_init_attr.cap.max_inline_data, SERVER_MAX_INLINE);
  return qp;
}

LC_INLINE void qp_init(struct ibv_qp* qp, int port)
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
    fprintf(stderr, "Unable to init qp\n");
    exit(EXIT_FAILURE);
  }
}

LC_INLINE void qp_to_rtr(struct ibv_qp* qp, int dev_port,
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
    fprintf(stderr, "failed to modify QP state to RTR\n");
    exit(EXIT_FAILURE);
  }
}

LC_INLINE void qp_to_rts(struct ibv_qp* qp)
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
    fprintf(stderr, "failed to modify QP state to RTS\n");
    exit(EXIT_FAILURE);
  }
}

LC_INLINE uintptr_t _real_ibv_reg(ibv_server* s, void* buf, size_t size)
{
  int mr_flags =
      IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;
  return (uintptr_t)ibv_reg_mr(s->dev_pd, buf, size, mr_flags);
}

LC_INLINE void _real_ibv_dereg(uintptr_t mem)
{
  ibv_dereg_mr((struct ibv_mr*)mem);
}

LC_INLINE uintptr_t ibv_rma_reg(ibv_server* s, void* buf, size_t size)
{
#ifdef USE_DREG
  return (uintptr_t)dreg_register(s, buf, size);
#else
  return _real_ibv_reg(s, buf, size);
#endif
}

LC_INLINE void ibv_rma_dereg(uintptr_t mem)
{
#ifdef USE_DREG
  dreg_unregister((dreg_entry*)mem);
#else
  _real_ibv_dereg(mem);
#endif
}

LC_INLINE uint32_t ibv_rma_key(uintptr_t mem)
{
#ifdef USE_DREG
  return ((struct ibv_mr*)(((dreg_entry*)mem)->memhandle[0]))->rkey;
#else
  return ((struct ibv_mr*)mem)->rkey;
#endif
}

LC_INLINE uint32_t ibv_rma_lkey(uintptr_t mem)
{
#ifdef USE_DREG
  return ((struct ibv_mr*)(((dreg_entry*)mem)->memhandle[0]))->lkey;
#else
  return ((struct ibv_mr*)mem)->lkey;
#endif
}

LC_INLINE void ibv_server_post_recv(ibv_server* s, lc_packet* p)
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

LC_INLINE int ibv_progress_recv_once(ibv_server* s)
{
  struct ibv_wc wc;
  int ret = ibv_poll_cq(s->recv_cq, 1, &wc);
  if (ret > 0) {
    return 1;
  }
  return 0;
}

LC_INLINE int ibv_progress_send_once(ibv_server* s)
{
  struct ibv_wc wc;
  int ret = ibv_poll_cq(s->send_cq, 1, &wc);
  if (ret > 0) {
    return 1;
  }
  return 0;
}

LC_INLINE int ibv_server_progress(ibv_server* s)
{  // profiler& p, long long& r, long long &s) {
  struct ibv_wc wc[MAX_CQ];
  int ret = 0;
  int ne = ibv_poll_cq(s->recv_cq, MAX_CQ, wc);

  if (ne > 0) {
    for (int i = 0; i < ne; i++) {
#ifdef LC_SERVER_DEBUG
      if (wc[i].status != IBV_WC_SUCCESS) {
        fprintf(stderr, "Failed status %s (%d) for wr_id %d\n",
                ibv_wc_status_str(wc[i].status), wc[i].status,
                (int)wc[i].wr_id);
        exit(EXIT_FAILURE);
      }
#endif
      s->recv_posted--;
      if (wc[i].opcode != IBV_WC_RECV_RDMA_WITH_IMM) {
        lc_packet* p = (lc_packet*)wc[i].wr_id;
        p->context.from = s->qp2rank[wc[i].qp_num % s->qp2rank_mod];
        p->context.size = wc[i].byte_len;
        p->context.tag = wc[i].imm_data >> 8;
        lc_serve_recv(s->mv, p, GET_PROTO(wc[i].imm_data));
      } else {
        lc_serve_imm(s->mv, wc[i].imm_data);
        lc_pool_put(s->mv->pkpool, (lc_packet*)wc[i].wr_id);
      }
    }
    ret = 1;
  }

  ne = ibv_poll_cq(s->send_cq, MAX_CQ, wc);

  if (ne > 0) {
    for (int i = 0; i < ne; i++) {
#ifdef LC_SERVER_DEBUG
      if (wc[i].status != IBV_WC_SUCCESS) {
        fprintf(stderr, "Failed status %s (%d) for wr_id %d\n",
                ibv_wc_status_str(wc[i].status), wc[i].status,
                (int)wc[i].wr_id);
        exit(EXIT_FAILURE);
      }
#endif
      lc_packet* p = (lc_packet*)wc[i].wr_id;
      if (p) lc_serve_send(s->mv, p, p->context.proto);
    }
    ret = 1;
  }

  // Make sure we always have enough packet, but do not block.
  if (s->recv_posted < MAX_RECV) {
    ibv_server_post_recv(s,
                         (lc_packet*)lc_pool_get_nb(s->mv->pkpool));  //, 0));
  }

#ifdef LC_SERVER_DEBUG
  if (s->recv_posted == 0) {
    fprintf(stderr, "WARNING DEADLOCK %d\n", s->recv_posted);
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

/*! This return whether or not to wait. */
LC_INLINE int ibv_server_write_send(ibv_server* s, int rank, void* ubuf,
                                    size_t size, lc_packet* ctx, uint32_t proto)
{
  struct ibv_sge list = {
      .addr = (uintptr_t)ubuf,   // address
      .length = (uint32_t)size,  // length
      .lkey = s->heap->lkey,     // lkey
  };

  struct ibv_send_wr this_wr;
  struct ibv_send_wr* bad_wr;
  if (size <= server_max_inline) {
    setup_wr(this_wr, (uintptr_t)0, &list, IBV_WR_SEND_WITH_IMM,
             IBV_SEND_INLINE | IBV_SEND_SIGNALED);
    this_wr.imm_data = proto;
    IBV_SAFECALL(ibv_post_send(s->dev_qp[rank], &this_wr, &bad_wr));
    lc_serve_send(s->mv, ctx, GET_PROTO(proto));
    return 0;
  } else {
    memcpy(ctx->data.buffer, ubuf, size);
    list.addr = (uintptr_t)ctx->data.buffer;
    ctx->context.proto = GET_PROTO(proto);
    setup_wr(this_wr, (uintptr_t)ctx, &list, IBV_WR_SEND_WITH_IMM,
             IBV_SEND_SIGNALED);
    this_wr.imm_data = proto;
    IBV_SAFECALL(ibv_post_send(s->dev_qp[rank], &this_wr, &bad_wr));
    return 1;
  }
}

LC_INLINE void ibv_server_read(ibv_server* s, int rank, void* dst,
                               uintptr_t src, uint32_t rkey, size_t size,
                               lc_packet* ctx, uint32_t proto)

{
  ctx->context.proto = proto;
  struct ibv_send_wr this_wr;
  struct ibv_send_wr* bad_wr = 0;

  struct ibv_sge list = {
      .addr = (uintptr_t)dst,    // address
      .length = (unsigned)size,  // length
      .lkey = s->heap->lkey,     // lkey
  };

  setup_wr(this_wr, (uintptr_t)ctx, &list, IBV_WR_RDMA_READ, IBV_SEND_SIGNALED);
  this_wr.wr.rdma.remote_addr = (uintptr_t)src;
  this_wr.wr.rdma.rkey = rkey;

  IBV_SAFECALL(ibv_post_send(s->dev_qp[rank], &this_wr, &bad_wr));
}

LC_INLINE void ibv_server_write_rma(ibv_server* s, int rank, void* from,
                                    uintptr_t to, uint32_t rkey, size_t size,
                                    lc_packet* ctx, uint32_t proto)

{
  ctx->context.proto = proto;
  struct ibv_send_wr this_wr;
  struct ibv_send_wr* bad_wr = 0;

  struct ibv_sge list = {
      .addr = (uintptr_t)from,   // address
      .length = (unsigned)size,  // length
      .lkey = s->heap->lkey,     // lkey
  };

  setup_wr(this_wr, (uintptr_t)ctx, &list, IBV_WR_RDMA_WRITE,
           IBV_SEND_SIGNALED);
  this_wr.wr.rdma.remote_addr = (uintptr_t)to;
  this_wr.wr.rdma.rkey = rkey;

  IBV_SAFECALL(ibv_post_send(s->dev_qp[rank], &this_wr, &bad_wr));
}

LC_INLINE void ibv_server_write_rma_signal(ibv_server* s, int rank, void* from,
                                           uintptr_t addr, uint32_t rkey,
                                           size_t size, uint32_t sid,
                                           lc_packet* ctx)
{
  struct ibv_send_wr this_wr;  // = {0};
  struct ibv_send_wr* bad_wr = 0;

  uintptr_t mr = ibv_rma_reg(s, from, size);

  struct ibv_sge list = {
      .addr = (uintptr_t)from,   // address
      .length = (unsigned)size,  // length
      .lkey = ibv_rma_lkey(mr),
  };

  setup_wr(this_wr, (uintptr_t)ctx, &list, IBV_WR_RDMA_WRITE_WITH_IMM,
           IBV_SEND_SIGNALED);

  this_wr.wr.rdma.remote_addr = addr;
  this_wr.wr.rdma.rkey = rkey;
  this_wr.imm_data = sid;

  IBV_SAFECALL(ibv_post_send(s->dev_qp[rank], &this_wr, &bad_wr));
}

LC_INLINE void ibv_server_init(lch* mv, size_t heap_size, ibv_server** s_ptr)
{
  ibv_server* s = 0;
  posix_memalign((void**)&s, 64, sizeof(ibv_server));
  assert(s);

  int num_devices;
  struct ibv_device** dev_list = ibv_get_device_list(&num_devices);
  if (num_devices <= 0) {
    fprintf(stderr, "Unable to find any ibv devices\n");
    exit(EXIT_FAILURE);
  }
  // Use the last one by default.
  s->dev_ctx = ibv_open_device(dev_list[num_devices - 1]);
  if (s->dev_ctx == 0) {
    fprintf(stderr, "Unable to find any ibv devices\n");
    exit(EXIT_FAILURE);
  }
  ibv_free_device_list(dev_list);

  struct ibv_device_attr dev_attr;
  int rc = ibv_query_device(s->dev_ctx, &dev_attr);
  if (rc != 0) {
    fprintf(stderr, "Unable to query device\n");
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
    fprintf(stderr, "Unable to query port\n");
    exit(EXIT_FAILURE);
  }

  s->dev_pd = ibv_alloc_pd(s->dev_ctx);
  if (s->dev_pd == 0) {
    fprintf(stderr, "Could not create protection domain for context\n");
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
    fprintf(stderr, "Could not create shared received queue\n");
    exit(EXIT_FAILURE);
  }

  // Create completion queues.
  s->send_cq = ibv_create_cq(s->dev_ctx, 64 * 1024, 0, 0, 0);
  s->recv_cq = ibv_create_cq(s->dev_ctx, 64 * 1024, 0, 0, 0);
  if (s->send_cq == 0 || s->recv_cq == 0) {
    fprintf(stderr, "Unable to create cq\n");
    exit(EXIT_FAILURE);
  }

  // Create RDMA memory.
  s->heap = ibv_server_mem_malloc(s, heap_size);
  s->heap_ptr = (void*)s->heap->addr;

  if (s->heap == 0) {
    fprintf(stderr, "Unable to create heap\n");
    exit(EXIT_FAILURE);
  }

  char* lc_mpi = getenv("LC_MPI");
  int with_mpi = s->with_mpi = 0;
  if (lc_mpi)
    with_mpi = s->with_mpi = atoi(lc_mpi);

  char key[256];
  char value[256];
  char name[256];

  if (with_mpi) {
    int provided;
    MPI_Init_thread(NULL, NULL, MPI_THREAD_FUNNELED, &provided);
    if (MPI_THREAD_FUNNELED != provided) {
      fprintf(stderr, "Need MPI_THREAD_MULTIPLE\n");
      exit(EXIT_FAILURE);
    }

    MPI_Comm_rank(MPI_COMM_WORLD, &mv->me);
    MPI_Comm_size(MPI_COMM_WORLD, &mv->size);
  } else {
    int spawned;
    PMI_Init(&spawned, &mv->size, &mv->me);
    PMI_KVS_Get_my_name(name, 255);
  }

  s->conn = (struct conn_ctx*)malloc(sizeof(struct conn_ctx) * mv->size);
  s->dev_qp = (struct ibv_qp**)malloc(sizeof(struct ibv_qp*) * mv->size);

  struct conn_ctx lctx;
  struct conn_ctx* rmtctx;

  for (int i = 0; i < mv->size; i++) {
    s->dev_qp[i] = qp_create(s, &dev_attr);
    if (!s->dev_qp[i]) {
      fprintf(stderr, "Unable to create queue pair\n");
      exit(EXIT_FAILURE);
    }

    rmtctx = &s->conn[i];
    lctx.addr = (uintptr_t)s->heap_ptr;
    lctx.rkey = s->heap->rkey;
    lctx.qp_num = s->dev_qp[i]->qp_num;
    lctx.lid = port_attr.lid;
    rc = ibv_query_gid(s->dev_ctx, dev_port, 0, &lctx.gid);
    if (rc != 0) {
      fprintf(stderr, "Unable to query gid\n");
      exit(EXIT_FAILURE);
    }
    if (with_mpi) {
      // Exchange connection conn_ctx.
      MPI_Sendrecv(&lctx, sizeof(struct conn_ctx), MPI_BYTE, i, 0, rmtctx,
          sizeof(struct conn_ctx), MPI_BYTE, i, 0, MPI_COMM_WORLD,
          MPI_STATUS_IGNORE);
      qp_init(s->dev_qp[i], dev_port);
      qp_to_rtr(s->dev_qp[i], dev_port, &port_attr, rmtctx);
      qp_to_rts(s->dev_qp[i]);
    } else {
      sprintf(key, "_LC_KEY_%d_%d", mv->me, i);
      sprintf(value, "%llu-%d-%d-%d", (unsigned long long) lctx.addr, lctx.rkey, lctx.qp_num, (int) lctx.lid);
      PMI_KVS_Put(name, key, value);
    }
  }

  if (!with_mpi) {
    PMI_Barrier();
    for (int i = 0; i < mv->size; i++) {
      sprintf(key, "_LC_KEY_%d_%d", i, mv->me);
      PMI_KVS_Get(name, key, value, 255);
      rmtctx = &s->conn[i];
      sscanf(value, "%llu-%d-%d-%d", (unsigned long long*) &rmtctx->addr, &rmtctx->rkey, &rmtctx->qp_num, (int*) &rmtctx->lid);
      qp_init(s->dev_qp[i], dev_port);
      qp_to_rtr(s->dev_qp[i], dev_port, &port_attr, rmtctx);
      qp_to_rts(s->dev_qp[i]);
    }
  }

  // ***HACK TO MAP QP TO RANK***
  int j = mv->size;
  int* b;
  while (1) {
    b = (int*)calloc(j, sizeof(int));
    int i = 0;
    for (; i < mv->size; i++) {
      int k = (s->dev_qp[i]->qp_num % j);
      if (b[k]) break;
      b[k] = 1;
    }
    if (i == mv->size) break;
    j++;
    free(b);
  }

  for (int i = 0; i < mv->size; i++) {
    b[s->dev_qp[i]->qp_num % j] = i;
  }
  s->qp2rank_mod = j;
  s->qp2rank = b;

#ifdef USE_DREG
  dreg_init();
#endif

  s->recv_posted = 0;
  s->mv = mv;
  *s_ptr = s;
}

LC_INLINE void ibv_server_finalize(ibv_server* s)
{
  ibv_destroy_cq(s->send_cq);
  ibv_destroy_cq(s->recv_cq);
  ibv_destroy_srq(s->dev_srq);
  ibv_server_mem_free(s->heap);
  for (int i = 0; i < s->mv->size; i++) {
    ibv_destroy_qp(s->dev_qp[i]);
  }
  if (s->with_mpi)
    MPI_Finalize();
  else
    PMI_Finalize();

  free(s->conn);
  free(s->dev_qp);
  free(s);
}

LC_INLINE lc_server_memory* ibv_server_mem_malloc(ibv_server* s, size_t size)
{
  int mr_flags =
      IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;
  void* ptr = 0;
  posix_memalign(&ptr, 4096, size + 4096);
  return ibv_reg_mr(s->dev_pd, ptr, size, mr_flags);
}

LC_INLINE void ibv_server_mem_free(lc_server_memory* mr)
{
  void* ptr = (void*)mr->addr;
  ibv_dereg_mr(mr);
  free(ptr);
}

LC_INLINE uint32_t ibv_server_heap_rkey(ibv_server* s, int node)
{
  return s->conn[node].rkey;
}

LC_INLINE void* ibv_server_heap_ptr(ibv_server* s) { return s->heap_ptr; }
#define lc_server_init ibv_server_init
#define lc_server_send ibv_server_write_send
#define lc_server_rma ibv_server_write_rma
#define lc_server_rma_signal ibv_server_write_rma_signal
#define lc_server_heap_rkey ibv_server_heap_rkey
#define lc_server_heap_ptr ibv_server_heap_ptr
#define lc_server_progress ibv_server_progress
#define lc_server_finalize ibv_server_finalize
#define lc_server_post_recv ibv_server_post_recv

#define lc_server_rma_reg ibv_rma_reg
#define lc_server_rma_key ibv_rma_key
#define lc_server_rma_dereg ibv_rma_dereg
#define _real_server_reg _real_ibv_reg
#define _real_server_dereg _real_ibv_dereg

#endif
