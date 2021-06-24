#ifndef SERVER_IBV_H_
#define SERVER_IBV_H_

#include <stdlib.h>
#include <stdio.h>
#include "infiniband/verbs.h"
#include "dreg.h"

#ifdef LCI_DEBUG
#define IBV_SAFECALL(x)                                               \
  {                                                                   \
    int err = (x);                                                    \
    if (err) {                                                        \
      fprintf(stderr, "err : %d (%s:%d)\n", err, __FILE__, __LINE__); \
      exit(EXIT_FAILURE);                                             \
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

#define MAX_POLL 16

typedef struct LCIDI_server_t {
  LCI_device_t device;
  int recv_posted;

  // Device fields.
  struct ibv_device **dev_list;
  struct ibv_device *ib_dev;
  struct ibv_context* dev_ctx;
  struct ibv_pd* dev_pd;
  struct ibv_device_attr dev_attr;
  struct ibv_port_attr port_attr;
  struct ibv_srq* dev_srq;
  struct ibv_cq* send_cq;
  struct ibv_cq* recv_cq;
  uint8_t dev_port;

  // Connections O(N)
  struct ibv_qp** qps;

  // Helper fields.
  int* qp2rank;
  int qp2rank_mod;
  size_t max_inline;
} LCIDI_server_t __attribute__((aligned(64)));

static inline uintptr_t _real_server_reg(LCID_server_t s, void* buf, size_t size)
{
  LCIDI_server_t *server = (LCIDI_server_t*) s;
  int mr_flags =
      IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;
  return (uintptr_t)ibv_reg_mr(server->dev_pd, buf, size, mr_flags);
}

static inline void _real_server_dereg(uintptr_t mem)
{
  ibv_dereg_mr((struct ibv_mr*)mem);
}

#ifdef USE_DREG
static inline LCID_mr_t lc_server_rma_reg(LCID_server_t s, void* buf, size_t size)
{
  return (LCID_mr_t)dreg_register(s, buf, size);
}

static inline void lc_server_rma_dereg(LCID_mr_t mr)
{
  dreg_unregister((dreg_entry*)mr);
}

static inline LCID_rkey_t lc_server_rma_rkey(LCID_mr_t mr)
{
  return ((struct ibv_mr*)(((dreg_entry*)mr)->memhandle[0]))->rkey;
}

static inline uint32_t ibv_rma_lkey(LCID_mr_t mr)
{
  return ((struct ibv_mr*)(((dreg_entry*)mr)->memhandle[0]))->lkey;
}

#else

static inline uintptr_t lc_server_rma_reg(LCID_server_t s, void* buf, size_t size)
{
  return _real_server_reg(s, buf, size);
}

static inline void lc_server_rma_dereg(uintptr_t mem)
{
  _real_server_dereg(mem);
}

static inline uint32_t lc_server_rma_rkey(uintptr_t mem)
{
  return ((struct ibv_mr*)mem)->rkey;
}

static inline uint32_t ibv_rma_lkey(uintptr_t mem)
{
  return ((struct ibv_mr*)mem)->lkey;
}

#endif

static inline void lc_server_post_recv(LCID_server_t s, void *buf, uint32_t size, LCID_mr_t mr, void *user_context);

static inline int lc_server_progress(LCID_server_t s)
{
  LCIDI_server_t *server = (LCIDI_server_t*) s;
  struct ibv_wc wc[MAX_POLL];
  int ne = ibv_poll_cq(server->recv_cq, MAX_POLL, wc);
  LCM_DBG_Assert(ne >= 0, "ibv_poll_cq returns error\n");
  int ret = (ne > 0);
  for (int i = 0; i < ne; i++) {
    LCM_DBG_Assert(wc[i].status == IBV_WC_SUCCESS,
                   "Failed status %s (%d) for wr_id %d\n",
                   ibv_wc_status_str(wc[i].status), wc[i].status,
                   (int)wc[i].wr_id);
    server->recv_posted--;
    if (wc[i].opcode == IBV_WC_RECV) {
      // two-sided recv.
      lc_packet* packet = (lc_packet*)wc[i].wr_id;
      int src_rank = server->qp2rank[wc[i].qp_num % server->qp2rank_mod];
      LCM_DBG_Log(LCM_LOG_DEBUG, "complete recv: packet %p rank %d size %u imm_data %u\n",
                  packet, src_rank, wc[i].byte_len, wc[i].imm_data);
      lc_serve_recv(packet, src_rank, wc[i].byte_len, wc[i].imm_data);
    } else {
      LCM_DBG_Assert(wc[i].opcode == IBV_WC_RECV_RDMA_WITH_IMM, "Unexpected opcode! %d\n", wc[i].opcode);
      LCM_DBG_Log(LCM_LOG_DEBUG, "complete writeImm: imm_data %u\n", wc[i].imm_data);
      LCII_free_packet((lc_packet*)wc[i].wr_id);
      lc_serve_rdma(wc[i].imm_data);
    }
  }

  ne = ibv_poll_cq(server->send_cq, MAX_POLL, wc);
  ret |= (ne > 0);

  LCM_DBG_Assert(ne >= 0, "ibv_poll_cq returns error!\n");
  for (int i = 0; i < ne; i++) {
    LCM_DBG_Assert(wc[i].status == IBV_WC_SUCCESS,
                   "Failed status %s (%d) for wr_id %d\n",
                   ibv_wc_status_str(wc[i].status), wc[i].status,
                   (int)wc[i].wr_id);
    LCM_DBG_Log(LCM_LOG_DEBUG, "complete send: wr_id %p\n", (void*) wc[i].wr_id);
    if (wc[i].wr_id == 0) continue;
    lc_serve_send((void*)wc[i].wr_id);
  }

  // Make sure we always have enough packet, but do not block.
  while (server->recv_posted < LC_SERVER_MAX_RCVS) {
    lc_packet *packet = lc_pool_get_nb(server->device->pkpool);
    if (packet == NULL) {
      if (server->recv_posted < LC_SERVER_MAX_RCVS / 2 && !g_server_no_recv_packets) {
        g_server_no_recv_packets = 1;
        LCM_DBG_Log(LCM_LOG_DEBUG, "WARNING-LC: deadlock alert. There is only"
                                   "%d packets left for post_recv\n", server->recv_posted);
      }
      break;
    }
    packet->context.poolid = lc_pool_get_local(server->device->pkpool);
    lc_server_post_recv(s, packet->data.address, LCI_MEDIUM_SIZE,
                        server->device->heap.segment->mr_p, packet);
    ret = 1;
  }
  if (server->recv_posted == LC_SERVER_MAX_RCVS && g_server_no_recv_packets)
    g_server_no_recv_packets = 0;

  return ret;
}

static inline void lc_server_post_recv(LCID_server_t s, void *buf, uint32_t size, LCID_mr_t mr, void *user_context)
{
  LCM_DBG_Log(LCM_LOG_DEBUG, "post recv: buf %p size %u lkey %u user_context %p\n",
              buf, size, ibv_rma_lkey(mr), user_context);
  LCIDI_server_t *server = (LCIDI_server_t*) s;
  struct ibv_sge list;
  list.addr	= (uint64_t) buf;
  list.length = size;
  list.lkey	= ibv_rma_lkey(mr);
  struct ibv_recv_wr wr;
  wr.wr_id	    = (uint64_t) user_context;
  wr.next       = NULL;
  wr.sg_list    = &list;
  wr.num_sge    = 1;
  struct ibv_recv_wr *bad_wr;
  IBV_SAFECALL(ibv_post_srq_recv(server->dev_srq, &wr, &bad_wr));
  ++server->recv_posted;
}

static inline LCI_error_t lc_server_sends(LCID_server_t s, int rank, void* buf,
                                          size_t size, LCID_meta_t meta)
{
  LCM_DBG_Log(LCM_LOG_DEBUG, "post sends: rank %d buf %p size %lu meta %d\n",
              rank, buf, size, meta);
  LCIDI_server_t *server = (LCIDI_server_t*) s;
  LCM_DBG_Assert(size <= server->max_inline, "%lu exceed the inline message size"
                                             "limit! %lu\n", size, server->max_inline);

  struct ibv_sge list;
  list.addr	= (uint64_t) buf;
  list.length   = size;
  list.lkey	= 0;
  struct ibv_send_wr wr;
  wr.wr_id	= 0;
  wr.next       = NULL;
  wr.sg_list    = &list;
  wr.num_sge    = 1;
  wr.opcode     = IBV_WR_SEND_WITH_IMM;
  wr.send_flags = IBV_SEND_INLINE;
  wr.imm_data   = meta;

  static int ninline = 0;
  __sync_fetch_and_add(&ninline, 1);
  if (ninline == 64) {
    wr.send_flags |= IBV_SEND_SIGNALED;
    ninline = 0;
  }

  struct ibv_send_wr *bad_wr;
  IBV_SAFECALL(ibv_post_send(server->qps[rank], &wr, &bad_wr));
  return LCI_OK;
}

static inline LCI_error_t lc_server_send(LCID_server_t s, int rank, void* buf,
                                         size_t size, LCID_mr_t mr, LCID_meta_t meta,
                                         void* ctx)
{
  LCM_DBG_Log(LCM_LOG_DEBUG, "post send: rank %d buf %p size %lu lkey %d meta %d ctx %p\n",
              rank, buf, size, ibv_rma_lkey(mr), meta, ctx);
  LCIDI_server_t *server = (LCIDI_server_t*) s;

  struct ibv_sge list;
  list.addr	= (uint64_t) buf;
  list.length   = size;
  list.lkey	= ibv_rma_lkey(mr);
  struct ibv_send_wr wr;
  wr.wr_id	= (uintptr_t) ctx;
  wr.next       = NULL;
  wr.sg_list    = &list;
  wr.num_sge    = 1;
  wr.opcode     = IBV_WR_SEND_WITH_IMM;
  wr.send_flags = IBV_SEND_SIGNALED;
  wr.imm_data   = meta;
  if (size <= server->max_inline) {
    wr.send_flags |= IBV_SEND_INLINE;
  }

  struct ibv_send_wr* bad_wr;
  IBV_SAFECALL(ibv_post_send(server->qps[rank], &wr, &bad_wr));
  return LCI_OK;
}

static inline LCI_error_t lc_server_puts(LCID_server_t s, int rank, void* buf,
                                         size_t size, uintptr_t base, uint32_t offset,
                                         LCID_rkey_t rkey, uint32_t meta)
{
  LCM_DBG_Log(LCM_LOG_DEBUG, "post puts: rank %d buf %p size %lu base %p offset %d "
              "rkey %lu meta %d\n", rank, buf,
              size, (void*) base, offset, rkey, meta);
  LCIDI_server_t *server = (LCIDI_server_t*) s;
  LCM_DBG_Assert(size <= server->max_inline, "%lu exceed the inline message size"
                                             "limit! %lu\n", size, server->max_inline);
  struct ibv_sge list;
  list.addr	= (uint64_t) buf;
  list.length   = size;
  list.lkey	= 0;
  struct ibv_send_wr wr;
  wr.wr_id	= 0;
  wr.next       = NULL;
  wr.sg_list    = &list;
  wr.num_sge    = 1;
  wr.opcode     = IBV_WR_RDMA_WRITE_WITH_IMM;
  wr.send_flags = IBV_SEND_SIGNALED | IBV_SEND_INLINE;
  wr.wr.rdma.remote_addr = (uintptr_t)(base + offset);
  wr.wr.rdma.rkey = rkey;
  wr.imm_data   = meta;

  struct ibv_send_wr *bad_wr;
  IBV_SAFECALL(ibv_post_send(server->qps[rank], &wr, &bad_wr));
  return LCI_OK;
}

static inline LCI_error_t lc_server_put(LCID_server_t s, int rank, void* buf,
                                        size_t size, LCID_mr_t mr, uintptr_t base,
                                        uint32_t offset, LCID_rkey_t rkey,
                                        LCID_meta_t meta, void* ctx)
{
  LCM_DBG_Log(LCM_LOG_DEBUG, "post put: rank %d buf %p size %lu lkey %u base %p "
              "offset %d rkey %lu meta %u ctx %p\n", rank, buf,
              size, ibv_rma_lkey(mr), (void*) base, offset, rkey, meta, ctx);
  LCIDI_server_t *server = (LCIDI_server_t*) s;

  struct ibv_sge list;
  list.addr	= (uint64_t) buf;
  list.length   = size;
  list.lkey	= ibv_rma_lkey(mr);
  struct ibv_send_wr wr;
  wr.wr_id      = (uint64_t) ctx;
  wr.next       = NULL;
  wr.sg_list    = &list;
  wr.num_sge    = 1;
  wr.opcode     = IBV_WR_RDMA_WRITE_WITH_IMM;
  wr.send_flags = IBV_SEND_SIGNALED;
  wr.imm_data   = meta;
  wr.wr.rdma.remote_addr = (uintptr_t)(base + offset);
  wr.wr.rdma.rkey = rkey;
  if (size <= server->max_inline) {
    wr.send_flags |= IBV_SEND_INLINE;
  }
  struct ibv_send_wr *bad_wr;

  IBV_SAFECALL(ibv_post_send(server->qps[rank], &wr, &bad_wr));
  return LCI_OK;
}

static inline int lc_server_recv_posted_num(LCID_server_t s) {
  LCIDI_server_t *server = (LCIDI_server_t*) s;
  return server->recv_posted;
}

#endif
