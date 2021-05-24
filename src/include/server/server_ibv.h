#ifndef SERVER_IBV_H_
#define SERVER_IBV_H_

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include "infiniband/verbs.h"

#include "pm.h"
#include "dreg.h"

typedef struct lc_server {
  SERVER_COMMON
  struct lc_dev* dev;

  // Device fields.
  struct ibv_context* dev_ctx;
  struct ibv_pd* dev_pd;
  struct ibv_srq* dev_srq;
  struct ibv_cq* send_cq;
  struct ibv_cq* recv_cq;
  struct ibv_mr* sbuf;
  struct ibv_mr* heap;

  struct ibv_port_attr port_attr;
  struct ibv_device_attr dev_attr;

  uint8_t dev_port;

  // Connections O(N)
  struct ibv_qp** qp;

  // Helper fields.
  int* qp2rank;
  int qp2rank_mod;
  size_t max_inline;
} lc_server __attribute__((aligned(64)));

#include "server_ibv_helper.h"

#ifdef USE_DREG
static inline LCID_mr_t lc_server_rma_reg(lc_server* s, void* buf, size_t size)
{
  return (LCID_mr_t)dreg_register(s, buf, size);
}

static inline void lc_server_rma_dereg(LCID_mr_t mr)
{
  dreg_unregister((dreg_entry*)mr);
}

static inline LCID_rkey_t lc_server_rma_key(LCID_mr_t mr)
{
  return ((struct ibv_mr*)(((dreg_entry*)mr)->memhandle[0]))->rkey;
}

static inline uint32_t ibv_rma_lkey(LCID_mr_t mr)
{
  return ((struct ibv_mr*)(((dreg_entry*)mr)->memhandle[0]))->lkey;
}

#else

static inline uintptr_t lc_server_rma_reg(lc_server* s, void* buf, size_t size)
{
  return _real_server_reg(s, buf, size);
}

static inline void lc_server_rma_dereg(uintptr_t mem)
{
  _real_server_dereg(mem);
}

static inline uint32_t lc_server_rma_key(uintptr_t mem)
{
  return ((struct ibv_mr*)mem)->rkey;
}

static inline uint32_t ibv_rma_lkey(uintptr_t mem)
{
  return ((struct ibv_mr*)mem)->lkey;
}

#endif

static inline int lc_server_progress(lc_server* s)
{
  struct ibv_wc wc[MAX_CQ];
  int ne = ibv_poll_cq(s->recv_cq, MAX_CQ, wc);
  int ret = (ne > 0);
  int i;

  LCM_DBG_Assert(ne >= 0, "ibv_poll_cq returns error");
  for (i = 0; i < ne; i++) {
    LCM_DBG_Assert(wc[i].status == IBV_WC_SUCCESS,
                   "Failed status %s (%d) for wr_id %d\n",
                   ibv_wc_status_str(wc[i].status), wc[i].status,
                   (int)wc[i].wr_id);
    s->recv_posted--;
    __builtin_prefetch((void*)wc[i].wr_id);
    if (wc[i].opcode == IBV_WC_RECV) {
      // two-sided recv.
      lc_packet* packet = (lc_packet*)wc[i].wr_id;
      int src_rank = s->qp2rank[wc[i].qp_num % s->qp2rank_mod];
      lc_serve_recv(packet, src_rank, wc[i].byte_len, wc[i].imm_data);
    } else {
      LCM_DBG_Assert(wc[i].opcode == IBV_WC_RECV_RDMA_WITH_IMM, "unexpected opcode");
      lc_serve_rdma(wc[i].imm_data);
    }
  }

  ne = ibv_poll_cq(s->send_cq, MAX_CQ, wc);
  ret |= (ne > 0);

  LCM_DBG_Assert(ne >= 0, "ibv_poll_cq returns error");
  for (i = 0; i < ne; i++) {
    LCM_DBG_Assert(wc[i].status == IBV_WC_SUCCESS,
                   "Failed status %s (%d) for wr_id %d\n",
                   ibv_wc_status_str(wc[i].status), wc[i].status,
                   (int)wc[i].wr_id);
    LCM_DBG_Assert(wc[i].wr_id != 0, "ibv send/write: don't receive any context");
    lc_serve_send((void*)wc[i].wr_id);
  }

  // Make sure we always have enough packet, but do not block.
  if (s->recv_posted < LC_SERVER_MAX_RCVS) {
    ibv_post_recv_(s, (lc_packet*)lc_pool_get_nb(s->pkpool));  //, 0));
    ret = 1;
  }

  LCM_DBG_Log(LCM_LOG_WARN, "WARNING DEADLOCK %lu\n", s->recv_posted);

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
  }

static inline void lc_server_sends(lc_server* s __UNUSED__, LCID_addr_t dest, void* buf,
                                   size_t size, LCID_meta_t meta)
{
  struct ibv_send_wr this_wr;
  struct ibv_send_wr* bad_wr;
  static int ninline = 0;

  struct ibv_sge list = {
      .addr = (uintptr_t)buf, .length = (uint32_t)size, .lkey = 0};

  setup_wr(this_wr, (uintptr_t)0, &list, IBV_WR_SEND_WITH_IMM,
           IBV_SEND_INLINE);  // NOTE: do not signal freqly here.

  if (unlikely(ninline++ == 64)) {
    this_wr.send_flags |= IBV_SEND_SIGNALED;
    ninline = 0;
  }

  this_wr.imm_data = meta;
  IBV_SAFECALL(ibv_post_send((struct ibv_qp*)dest, &this_wr, &bad_wr));
}

static inline void lc_server_send(lc_server* s __UNUSED__, LCID_addr_t dest, void* buf,
                                  size_t size, LCID_mr_t mr, LCID_meta_t meta,
                                  void* ctx)
{
  struct ibv_send_wr this_wr;
  struct ibv_send_wr* bad_wr;

  struct ibv_sge list = {
      .addr = (uintptr_t)buf,
      .length = (uint32_t)size,
      .lkey = ibv_rma_lkey(mr),
  };

  setup_wr(this_wr, (uintptr_t)ctx, &list, IBV_WR_SEND_WITH_IMM,
           IBV_SEND_SIGNALED);
  this_wr.imm_data = meta;
  IBV_SAFECALL(ibv_post_send((struct ibv_qp*) dest, &this_wr, &bad_wr));
}

static inline void lc_server_puts(lc_server* s __UNUSED__, LCID_addr_t dest, void* buf,
                                  size_t size, uintptr_t base, uint32_t offset,
                                  LCID_rkey_t rkey, uint32_t meta)
{
  struct ibv_send_wr this_wr;
  struct ibv_send_wr* bad_wr = 0;

  struct ibv_sge list = {
      .addr = (uintptr_t)buf,
      .length = (unsigned)size,
      .lkey = 0,
  };

  setup_wr(this_wr, 0, &list, IBV_WR_RDMA_WRITE_WITH_IMM,
           IBV_SEND_SIGNALED | IBV_SEND_INLINE);
  this_wr.wr.rdma.remote_addr = (uintptr_t)(base + offset);
  this_wr.wr.rdma.rkey = rkey;
  this_wr.imm_data = meta;

  IBV_SAFECALL(ibv_post_send(dest, &this_wr, &bad_wr));
}

static inline void lc_server_put(lc_server* s, LCID_addr_t dest, void* buf,
                                 size_t size, LCID_mr_t mr, uintptr_t base,
                                 uint32_t offset, LCID_rkey_t rkey,
                                 LCID_meta_t meta, void* ctx)
{
  struct ibv_send_wr this_wr;
  struct ibv_send_wr* bad_wr = 0;

  struct ibv_sge list = {
      .addr = (uintptr_t)buf,
      .length = (unsigned)size,
      .lkey = ibv_rma_lkey(mr),
  };

  setup_wr(this_wr, (uintptr_t)ctx, &list, IBV_WR_RDMA_WRITE_WITH_IMM,
           IBV_SEND_SIGNALED);
  this_wr.wr.rdma.remote_addr = (uintptr_t)(base + offset);
  this_wr.wr.rdma.rkey = rkey;
  this_wr.imm_data = meta;

  IBV_SAFECALL(ibv_post_send(dest, &this_wr, &bad_wr));
}

static inline void lc_server_rma_rtr(lc_server* s, void* rep, void* buf,
                                     uintptr_t addr, uint64_t rkey, size_t size,
                                     uint32_t sid, lc_packet* ctx)
{
  struct ibv_send_wr this_wr;
  struct ibv_send_wr* bad_wr = 0;
  uint32_t lkey = 0;
  uint32_t flag = IBV_SEND_SIGNALED;

  if (size > s->max_inline) {
    lkey = ibv_rma_lkey(lc_server_rma_reg(s, buf, size));
  } else {
    flag |= IBV_SEND_INLINE;
  }

  struct ibv_sge list = {
      .addr = (uintptr_t)buf,
      .length = (unsigned)size,
      .lkey = lkey,
  };

  setup_wr(this_wr, (uintptr_t)ctx, &list, IBV_WR_RDMA_WRITE_WITH_IMM, flag);
  this_wr.wr.rdma.remote_addr = (uintptr_t)addr;
  this_wr.wr.rdma.rkey = rkey;
  this_wr.imm_data = sid | IBV_IMM_RTR;

  IBV_SAFECALL(ibv_post_send(rep, &this_wr, &bad_wr));
}

static inline void lc_server_init(int id, lc_server** dev)
{
  lc_server* s = NULL;
  posix_memalign((void**)&s, 8192, sizeof(struct lc_server));

  *dev = s;
  s->id = id;

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

  struct ibv_device_attr* dev_attr = &s->dev_attr;
  int rc = ibv_query_device(s->dev_ctx, dev_attr);
  if (rc != 0) {
    fprintf(stderr, "Unable to query device\n");
    exit(EXIT_FAILURE);
  }

  struct ibv_port_attr* port_attr = &s->port_attr;
  uint8_t dev_port = 0;
  for (; dev_port < 128; dev_port++) {
    rc = ibv_query_port(s->dev_ctx, dev_port, port_attr);
    if (rc == 0) break;
  }
  s->dev_port = dev_port;

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
  srq_attr.attr.max_wr = LC_SERVER_MAX_RCVS;
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
  s->heap = ibv_mem_malloc(s, LC_SERVER_NUM_PKTS * LC_PACKET_SIZE * 2 + LCI_REGISTERED_SEGMENT_SIZE);
  s->heap_addr = (uintptr_t)s->heap->addr;

  if (s->heap == 0) {
    fprintf(stderr, "Unable to create heap\n");
    exit(EXIT_FAILURE);
  }

  s->recv_posted = 0;
  posix_memalign((void**)&s->qp, LC_CACHE_LINE,
                 LCI_NUM_PROCESSES * sizeof(struct ibv_qp*));

  struct conn_ctx lctx, rctx;
  char ep_name[256];
  lctx.addr = (uintptr_t)s->heap_addr;
  lctx.rkey = s->heap->rkey;
  lctx.lid = s->port_attr.lid;

  for (int i = 0; i < LCI_NUM_PROCESSES; i++) {
    s->qp[i] = qp_create(s);
    qp_init(s->qp[i], s->dev_port);
    // Use this endpoint "i" to connect to rank e.
    lctx.qp_num = s->qp[i]->qp_num;
    sprintf(ep_name, "%llu-%d-%d-%d", (unsigned long long)lctx.addr, lctx.rkey,
            lctx.qp_num, (int)lctx.lid);
    lc_pm_publish(LCI_RANK, (s->id) << 8 | i, ep_name);
  }

  posix_memalign((void**)&(s->rep), LC_CACHE_LINE,
                 sizeof(struct lc_rep) * LCI_NUM_PROCESSES);

  for (int i = 0; i < LCI_NUM_PROCESSES; i++) {
    lc_pm_getname(i, (s->id << 8) | LCI_RANK, ep_name);
    sscanf(ep_name, "%llu-%d-%d-%d", (unsigned long long*)&rctx.addr,
           &rctx.rkey, &rctx.qp_num, (int*)&rctx.lid);
    qp_to_rtr(s->qp[i], s->dev_port, &s->port_attr, &rctx);
    qp_to_rts(s->qp[i]);

    struct lc_rep* rep = &s->rep[i];
    rep->rank = i;
    rep->rkey = rctx.rkey;
    rep->handle = (void*)s->qp[i];
    rep->base = rctx.addr;
  }

  int j = LCI_NUM_PROCESSES;
  int* b;
  while (1) {
    b = (int*)calloc(j, sizeof(int));
    int i = 0;
    for (; i < LCI_NUM_PROCESSES; i++) {
      int k = (s->qp[i]->qp_num % j);
      if (b[k]) break;
      b[k] = 1;
    }
    if (i == LCI_NUM_PROCESSES) break;
    j++;
    free(b);
  }
  for (int i = 0; i < LCI_NUM_PROCESSES; i++) {
    b[s->qp[i]->qp_num % j] = i;
  }
  s->qp2rank_mod = j;
  s->qp2rank = b;

#ifdef USE_DREG
  dreg_init();
#endif
  lc_pm_barrier();
}

static inline void lc_server_finalize(lc_server* s)
{
  ibv_destroy_cq(s->send_cq);
  ibv_destroy_cq(s->recv_cq);
  ibv_destroy_srq(s->dev_srq);
  free(s);
}

#define lc_server_post_rma(...) \
  {                             \
  }

#endif
