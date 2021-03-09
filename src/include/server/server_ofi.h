#ifndef SERVER_OFI_H_
#define SERVER_OFI_H_

#include "config.h"

#include <rdma/fabric.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_tagged.h>
#include <stdlib.h>
#include <string.h>

#include <rdma/fi_cm.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_rma.h>

#include "config.h"
#include "lciu.h"
#include "pm.h"
#include "dreg.h"

#ifdef LCI_DEBUG
#define FI_SAFECALL(x)                                                    \
  {                                                                       \
    int err = (x);                                                        \
    if (err < 0) err = -err;                                              \
    if (err) {                                                            \
      printf("err : %s (%s:%d)\n", fi_strerror(err), __FILE__, __LINE__); \
      exit(err);                                                          \
    }                                                                     \
  }                                                                       \
  while (0)                                                               \
    ;

#else
#define FI_SAFECALL(x) \
  {                    \
    (x);               \
  }
#endif

#define ALIGNMENT (4096)
#define ALIGNEDX(x) \
  (void*)((((uintptr_t)x + ALIGNMENT - 1) / ALIGNMENT * ALIGNMENT))
#define MAX_CQ_SIZE (16 * 1024)
#define MAX_POLL 8
#define OFI_IMM_RTR ((uint64_t)1 << 31)

typedef struct lc_server {
  SERVER_COMMON
  struct fi_info* fi;
  struct fid_fabric* fabric;
  struct fid_domain* domain;
  struct fid_ep* ep;
  struct fid_cq* cq;
  struct fid_mr* mr_heap;
  struct fid_av* av;
  void* mr_desc;
} lc_server __attribute__((aligned(64)));

static inline LCID_mr_t _real_server_reg(lc_server* s, void* buf, size_t size)
{
  struct fid_mr* mr;
  FI_SAFECALL(fi_mr_reg(s->domain, buf, size,
                        FI_READ | FI_WRITE | FI_REMOTE_WRITE, 0, lc_next_rdma_key++, 0,
                        &mr, 0));
  return (uintptr_t)mr;
}

static inline void _real_server_dereg(LCID_mr_t mr)
{
  FI_SAFECALL(fi_close((struct fid*) mem));
}

static inline LCID_mr_t lc_server_rma_reg(lc_server* s, void* buf, size_t size)
{
//  return (uintptr_t)dreg_register(s, buf, size);
   return _real_server_reg(s, buf, size);
}

static inline void lc_server_rma_dereg(LCID_mr_t mr)
{
//  dreg_unregister((dreg_entry*)mem);
  _real_server_dereg(mr);
}

static inline LCID_rkey_t lc_server_rma_key(LCID_mr_t mr)
{
//  return fi_mr_key((struct fid_mr*)(((dreg_entry*)mem)->memhandle[0]));
  return fi_mr_key((struct fid_mr*)(mr));
}

static inline void lc_server_init(int id, lc_server** dev)
{
  lc_server* s = NULL;
  posix_memalign((void**)&s, 8192, sizeof(struct lc_server));
  *dev = s;

  // Create hint.
  struct fi_info* hints;
  hints = fi_allocinfo();
  hints->ep_attr->type = FI_EP_RDM;
//  hints->domain_attr->mr_mode = FI_MR_BASIC;
  hints->domain_attr->mr_mode = FI_MR_VIRT_ADDR | FI_MR_ALLOCATED | FI_MR_PROV_KEY | FI_MR_LOCAL;
  hints->domain_attr->threading = FI_THREAD_SAFE;
  hints->domain_attr->control_progress = FI_PROGRESS_MANUAL;
  hints->domain_attr->data_progress = FI_PROGRESS_MANUAL;
  hints->caps = FI_RMA | FI_TAGGED;
  hints->mode = FI_LOCAL_MR;

  // Create info.
  FI_SAFECALL(fi_getinfo(FI_VERSION(1, 6), NULL, NULL, 0, hints, &s->fi));
  LCI_Log(LCI_LOG_INFO, "Provider name: %s\n", s->fi->fabric_attr->prov_name);
  LCI_Log(LCI_LOG_INFO, "MR mode hints: [%s]\n", fi_tostr(&(hints->domain_attr->mr_mode), FI_TYPE_MR_MODE));
  LCI_Log(LCI_LOG_INFO, "MR mode provided: [%s]\n", fi_tostr(&(s->fi->domain_attr->mr_mode), FI_TYPE_MR_MODE));
  LCI_Log(LCI_LOG_INFO, "Thread mode: %s\n", fi_tostr(&(s->fi->domain_attr->threading), FI_TYPE_THREADING));
  LCI_Log(LCI_LOG_INFO, "Control progress mode: %s\n", fi_tostr(&(s->fi->domain_attr->control_progress), FI_TYPE_PROGRESS));
  LCI_Log(LCI_LOG_INFO, "Data progress mode: %s\n", fi_tostr(&(s->fi->domain_attr->data_progress), FI_TYPE_PROGRESS));
  LCI_Log(LCI_LOG_MAX, "Fi_info provided: %s\n", fi_tostr(s->fi, FI_TYPE_INFO));
  LCI_Log(LCI_LOG_MAX, "Fabric attributes: %s\n", fi_tostr(s->fi->fabric_attr, FI_TYPE_FABRIC_ATTR));
  LCI_Log(LCI_LOG_MAX, "Domain attributes: %s\n", fi_tostr(s->fi->domain_attr, FI_TYPE_DOMAIN_ATTR));
  LCI_Log(LCI_LOG_MAX, "Endpoint attributes: %s\n", fi_tostr(s->fi->ep_attr, FI_TYPE_EP_ATTR));
  LCI_Assert(s->fi->domain_attr->cq_data_size >= 4, "cq_data_size = %lu\n", s->fi->domain_attr->cq_data_size);
  LCI_Assert(s->fi->domain_attr->mr_key_size <= 8, "mr_key_size = %lu\n", s->fi->domain_attr->mr_key_size);
  fi_freeinfo(hints);

  // Create libfabric obj.
  FI_SAFECALL(fi_fabric(s->fi->fabric_attr, &s->fabric, NULL));

  // Create domain.
  FI_SAFECALL(fi_domain(s->fabric, s->fi, &s->domain, NULL));

  // Create end-point;
  FI_SAFECALL(fi_endpoint(s->domain, s->fi, &s->ep, NULL));

  // Create cq.
  struct fi_cq_attr cq_attr;
  memset(&cq_attr, 0, sizeof(struct fi_cq_attr));
  cq_attr.format = FI_CQ_FORMAT_TAGGED;
  cq_attr.size = MAX_CQ_SIZE;
  FI_SAFECALL(fi_cq_open(s->domain, &cq_attr, &s->cq, NULL));

  // Bind my ep to cq.
  FI_SAFECALL(fi_ep_bind(s->ep, (fid_t)s->cq, FI_TRANSMIT | FI_RECV));

//  dreg_init();

  // Get memory for heap.
  s->heap_addr = 0; 
  posix_memalign((void**)&s->heap_addr, 4096, LC_SERVER_NUM_PKTS * LC_PACKET_SIZE * 2 + LCI_REGISTERED_SEGMENT_SIZE);

  FI_SAFECALL(fi_mr_reg(s->domain, (const void *) s->heap_addr, LC_SERVER_NUM_PKTS * LC_PACKET_SIZE * 2 + LCI_REGISTERED_SEGMENT_SIZE,
                        FI_READ | FI_WRITE | FI_REMOTE_WRITE, 0, 0, 0,
                        &s->mr_heap, 0));

  s->mr_desc = fi_mr_desc(s->mr_heap);
  s->id = id;

  struct fi_av_attr av_attr;
  memset(&av_attr, 0, sizeof(av_attr));
  av_attr.type = FI_AV_MAP;
  FI_SAFECALL(fi_av_open(s->domain, &av_attr, &s->av, NULL));
  FI_SAFECALL(fi_ep_bind(s->ep, (fid_t)s->av, 0));
  FI_SAFECALL(fi_enable(s->ep));

  // Now exchange end-point address, heap address, and rkey.
  // assume the size of the raw address no larger than 128 bits.
  const int EP_ADDR_LEN = 6;
  size_t addrlen = 0;
  fi_getname((fid_t)s->ep, NULL, &addrlen);
  LCI_Log(LCI_LOG_INFO, "addrlen = %lu\n", addrlen);
  LCI_Assert(addrlen <= 8 * EP_ADDR_LEN, "addrlen = %lu\n", addrlen);
  uint64_t my_addr[EP_ADDR_LEN];
  FI_SAFECALL(fi_getname((fid_t)s->ep, my_addr, &addrlen));
  uint64_t my_rkey = fi_mr_key(s->mr_heap);

  posix_memalign((void**)&(s->rep), LC_CACHE_LINE,
                 sizeof(struct lc_rep) * LCI_NUM_PROCESSES);
  uintptr_t heap_addr;
  if (s->fi->domain_attr->mr_mode & FI_MR_VIRT_ADDR || s->fi->domain_attr->mr_mode & FI_MR_BASIC) {
    LCI_Log(LCI_LOG_INFO, "FI_MR_VIRT_ADDR is set.\n");
    heap_addr = s->heap_addr;
  } else {
    LCI_Log(LCI_LOG_INFO, "FI_MR_VIRT_ADDR is not set.\n");
    heap_addr = 0;
  }
  char msg[256];
  const char* PARSE_STRING = "%016lx-%016lx-%016lx-%016lx-%016lx-%016lx-%lx-%lx";
  sprintf(msg, PARSE_STRING,
          my_addr[0], my_addr[1], my_addr[2], my_addr[3], my_addr[4], my_addr[5],
          heap_addr, my_rkey);
  lc_pm_publish(LCI_RANK, id, msg);

  for (int i = 0; i < LCI_NUM_PROCESSES; i++) {
    if (i != LCI_RANK) {
      lc_pm_getname(i, id, msg);
      uint64_t peer_addr[EP_ADDR_LEN];

      struct lc_rep* rep = &s->rep[i];
      rep->rank = i;
      posix_memalign((void**)&(rep->handle), LC_CACHE_LINE, sizeof(fi_addr_t));

      sscanf(msg, PARSE_STRING,
             &peer_addr[0], &peer_addr[1], &peer_addr[2], &peer_addr[3], &peer_addr[4], &peer_addr[5],
             &rep->base, &rep->rkey);
      int ret = fi_av_insert(s->av, (void*)peer_addr, 1, rep->handle, 0, NULL);
      LCI_Assert(ret == 1, "ret = %d\n", ret);
    } else {
      struct lc_rep* rep = &s->rep[i];
      rep->rank = i;
      posix_memalign((void**)&(rep->handle), LC_CACHE_LINE, sizeof(fi_addr_t));
      rep->base = heap_addr;
      rep->rkey = my_rkey;
      int ret = fi_av_insert(s->av, (void*)my_addr, 1, rep->handle, 0, NULL);
      LCI_Assert(ret == 1, "ret = %d\n", ret);
    }
  }

  s->recv_posted = 0;
}

static inline void lc_server_finalize(lc_server* s)
{
  free(s);
}

static inline void lc_server_post_recv(lc_server* s, lc_packet* p);

static inline int lc_server_progress(lc_server* s)
{
  // double t1 = -(MPI_Wtime());
  struct fi_cq_tagged_entry entry[MAX_POLL];
  struct fi_cq_err_entry error;
  ssize_t ret;
  int rett = 0;

  do {
    ret = fi_cq_read(s->cq, &entry, MAX_POLL);
    // t1 += MPI_Wtime();
    if (ret > 0) {
      // Got an entry here ?
      for (int i = 0; i < ret; i++) {
        if (entry[i].flags & FI_RECV) {
          s->recv_posted--;
          // we use tag to pass src_rank, because it is hard to get src_rank
          // from fi_addr_t. TODO: Need to improve
          lc_serve_recv(entry[i].op_context, entry[i].tag, entry[i].len,
                        entry[i].data);
        } else if (entry[i].flags & FI_REMOTE_WRITE) {
          // NOTE(danghvu): In OFI, a RDMA with completion data is transferred without
          // comsuming a posted receive.
          if (entry[i].data & OFI_IMM_RTR) {
            // RDMA immediate protocol (3-msg rdz).
            lc_packet* p = (lc_packet*)(s->heap_addr + (entry[i].data ^ OFI_IMM_RTR));
            lc_serve_imm(p);
          } else {
            // recv rdma with signal
            // TODO: this is dumb. try to modify proto.h to have some smarter functions here.
            lc_packet *p = lc_pool_get(s->pkpool);
            p->context.sync = &p->context.sync_s;
            lc_serve_recv_rdma(p, entry[i].data);
          }
        } else if (entry[i].flags & FI_SEND) {
          lc_serve_send(entry[i].op_context);
        } else if (entry[i].flags & FI_WRITE) {
          lc_packet* p = (lc_packet*)entry[i].op_context;
          lc_serve_send(p);
        }
      }
      rett = 1;
#ifdef LCI_DEBUG
    } else if (ret == -FI_EAGAIN) {
    } else {
      LCI_DBG_Assert(ret == -FI_EAVAIL, "unexpected return error: %s\n", fi_strerror(-ret));
      fi_cq_readerr(s->cq, &error, 0);
      printf("Err: %s\n", fi_strerror(error.err));
      exit(error.err);
#endif
    }
  } while (ret > 0);

  if (s->recv_posted < LC_SERVER_MAX_RCVS)
    lc_server_post_recv(s, lc_pool_get_nb(s->pkpool));

#ifdef LCI_DEBUG
  if (s->recv_posted == 0) {
    LCI_DBG_Log(LCI_LOG_WARN, "Run out of posted receive packets! Deadlock!\n");
  }
#endif

  return rett;
}

static inline void lc_server_post_recv(lc_server* s, lc_packet* p)
{
  if (p == NULL) return;
  FI_SAFECALL(
      fi_trecv(s->ep, &p->data, LCI_MEDIUM_SIZE, 0, FI_ADDR_UNSPEC, /*any*/0, ~(uint64_t)0 /*ignore all*/, p));
  s->recv_posted++;
}

static inline void lc_server_sends(lc_server* s, LCID_addr_t dest, void* buf,
                                   size_t size, LCID_meta_t meta)
{
  int ret;
  do {
    ret = fi_tinjectdata(s->ep, buf, size, meta, *(fi_addr_t*) dest, LCI_RANK /*tag*/);
  } while (ret == -FI_EAGAIN);
  if (ret) FI_SAFECALL(ret);
}

static inline void lc_server_send(lc_server* s, LCID_addr_t dest, void* buf,
                                  size_t size, LCID_mr_t mr, LCID_meta_t meta,
                                  void* ctx)
{
  int ret;
  do {
    ret = fi_tsenddata(s->ep, buf, size, fi_mr_desc((struct fid_mr*)mr), meta,
                       *(fi_addr_t*) dest, LCI_RANK /*tag*/,
                       (struct fi_context*)ctx);
  } while (ret == -FI_EAGAIN);
  if (ret) FI_SAFECALL(ret);
}

static inline void lc_server_puts(lc_server* s, LCID_addr_t dest, void* buf,
                                  size_t size, uintptr_t base, uint32_t offset,
                                  LCID_rkey_t rkey, uint32_t meta)
{
  int ret;
  do {
    ret = fi_inject_writedata(s->ep, buf, size, meta, *(fi_addr_t*)dest, base + offset, rkey);
  } while (ret == -FI_EAGAIN);
  if (ret) FI_SAFECALL(ret);
}

static inline void lc_server_put(lc_server* s, LCID_addr_t dest, void* buf,
                                  size_t size, LCID_mr_t mr, uintptr_t base,
                                  uint32_t offset, LCID_rkey_t rkey,
                                  LCID_meta_t meta, void* ctx)
{
  int ret;
  do {
    ret = fi_writedata(s->ep, buf, size, fi_mr_desc((struct fid_mr*)mr), meta,
                       *(fi_addr_t*) dest, base + offset, rkey, ctx);
  } while (ret == -FI_EAGAIN);
  if (ret) FI_SAFECALL(ret);
}

static inline void lc_server_rma_rtr(lc_server* s, void* rep, void* buf,
                                     uintptr_t addr, uint64_t rkey,
                                     size_t size, uint32_t sid, lc_packet* ctx)
{
  // workaround without modifying server.h
  if (!(s->fi->domain_attr->mr_mode & FI_MR_VIRT_ADDR || s->fi->domain_attr->mr_mode & FI_MR_BASIC)) {
    addr = 0;
  }
  int ret;
  do {
    ret = fi_writedata(s->ep, (void*)ctx->data.rts.src_addr, size, s->mr_desc /*might not always be true*/,
                       (uint64_t)sid | OFI_IMM_RTR, *(fi_addr_t*) rep, addr, rkey, ctx);
  } while (ret == -FI_EAGAIN);
  if (ret) FI_SAFECALL(ret);
}

#endif
