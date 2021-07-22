#ifndef SERVER_OFI_H_
#define SERVER_OFI_H_

#include <rdma/fabric.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_tagged.h>
#include <stdlib.h>
#include <string.h>

#include <rdma/fi_cm.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_rma.h>

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

#define MAX_CQ_SIZE (16 * 1024)
#define MAX_POLL 8

typedef struct LCIDI_server_t {
  LCI_device_t device;
  int recv_posted;
  struct fi_info* info;
  struct fid_fabric* fabric;
  struct fid_domain* domain;
  struct fid_ep* ep;
  struct fid_cq* cq;
  struct fid_av* av;
  fi_addr_t *peer_addrs;
  void *heap_desc;
} LCIDI_server_t __attribute__((aligned(64)));

extern int g_next_rdma_key;

static inline LCID_mr_t _real_server_reg(LCID_server_t s, void* buf, size_t size)
{
  LCIDI_server_t *server = (LCIDI_server_t*) s;
  struct fid_mr* mr;
  int rdma_key = __sync_fetch_and_add(&g_next_rdma_key, 1);
  FI_SAFECALL(fi_mr_reg(server->domain, buf, size,
                        FI_READ | FI_WRITE | FI_REMOTE_WRITE, 0, rdma_key, 0,
                        &mr, 0));
  return (uintptr_t)mr;
}

static inline void _real_server_dereg(LCID_mr_t mr_opaque)
{
  struct fid_mr *mr = (struct fid_mr*) mr_opaque;
  FI_SAFECALL(fi_close((struct fid*) &mr->fid));
}

static inline LCID_mr_t lc_server_rma_reg(LCID_server_t s, void* buf, size_t size)
{
//  return (uintptr_t)dreg_register(s, buf, size);
  return _real_server_reg(s, buf, size);
}

static inline void lc_server_rma_dereg(LCID_mr_t mr)
{
//  dreg_unregister((dreg_entry*)mem);
  _real_server_dereg(mr);
}

static inline LCID_rkey_t lc_server_rma_rkey(LCID_mr_t mr)
{
//  return fi_mr_key((struct fid_mr*)(((dreg_entry*)mem)->memhandle[0]));
  return fi_mr_key((struct fid_mr*)(mr));
}

static inline int lc_server_progress(LCID_server_t s)
{
  LCIDI_server_t *server = (LCIDI_server_t*) s;
  struct fi_cq_tagged_entry entry[MAX_POLL];
  struct fi_cq_err_entry error;
  ssize_t ne;
  int ret = 0;

  do {
    ne = fi_cq_read(server->cq, &entry, MAX_POLL);
    if (ne > 0) {
      // Got an entry here
      for (int i = 0; i < ne; i++) {
        if (entry[i].flags & FI_RECV) {
          --server->recv_posted;
          // we use tag to pass src_rank, because it is hard to get src_rank
          // from fi_addr_t. TODO: Need to improve
          lc_serve_recv(entry[i].op_context, entry[i].tag, entry[i].len,
                        entry[i].data);
        } else if (entry[i].flags & FI_REMOTE_WRITE) {
          lc_serve_rdma(entry[i].data);
        } else if (entry[i].flags & FI_SEND || entry[i].flags & FI_WRITE) {
          lc_serve_send(entry[i].op_context);
        }
      }
      ret |= (ne > 0);
#ifdef LCI_DEBUG
    } else if (ne == -FI_EAGAIN) {
    } else {
      LCM_DBG_Assert(ne == -FI_EAVAIL, "unexpected return error: %s\n", fi_strerror(-ne));
      fi_cq_readerr(server->cq, &error, 0);
      printf("Err: %s\n", fi_strerror(error.err));
      exit(error.err);
#endif
    }
  } while (ne > 0);

  while (server->recv_posted < LC_SERVER_MAX_RCVS) {
    lc_packet *packet = lc_pool_get_nb(server->device->pkpool);
    if (packet == NULL) break;
    FI_SAFECALL(fi_trecv(server->ep, &packet->data, LCI_MEDIUM_SIZE,
                           (void*) server->device->heap.segment->mr_p,
                           FI_ADDR_UNSPEC, /*any*/0, ~(uint64_t)0 /*ignore all*/,
                           packet));
    server->recv_posted++;
    ret = 1;
  }

  if (server->recv_posted == 0) {
    LCM_DBG_Log(LCM_LOG_WARN, "Run out of posted receive packets! Potentially deadlock!\n");
  }

  return ret;
}

static inline LCI_error_t lc_server_sends(LCID_server_t s, int rank, void* buf,
                                          size_t size, LCID_meta_t meta)
{
  LCIDI_server_t *server = (LCIDI_server_t*) s;
  int ret = fi_tinjectdata(server->ep, buf, size, meta, server->peer_addrs[rank],
                           LCI_RANK /*tag*/);
  if (ret == FI_SUCCESS) return LCI_OK;
  else if (ret == -FI_EAGAIN) return LCI_ERR_RETRY;
  else FI_SAFECALL(ret);
}

static inline LCI_error_t lc_server_send(LCID_server_t s, int rank, void* buf,
                                         size_t size, LCID_mr_t mr, LCID_meta_t meta,
                                         void* ctx)
{
  LCIDI_server_t *server = (LCIDI_server_t*) s;
  int ret = fi_tsenddata(server->ep, buf, size, fi_mr_desc((struct fid_mr*)mr),
                         meta, server->peer_addrs[rank], LCI_RANK /*tag*/,
                         (struct fi_context*)ctx);
  if (ret == FI_SUCCESS) return LCI_OK;
  else if (ret == -FI_EAGAIN) return LCI_ERR_RETRY;
  else FI_SAFECALL(ret);
}

static inline LCI_error_t lc_server_puts(LCID_server_t s, int rank, void* buf,
                                         size_t size, uintptr_t base, uint32_t offset,
                                         LCID_rkey_t rkey, uint32_t meta)
{
  LCIDI_server_t *server = (LCIDI_server_t*) s;
  uintptr_t addr;
  if (server->info->domain_attr->mr_mode & FI_MR_VIRT_ADDR || server->info->domain_attr->mr_mode & FI_MR_BASIC) {
    addr = base + offset;
  } else {
    addr = offset;
  }
  int ret = fi_inject_writedata(server->ep, buf, size, meta, server->peer_addrs[rank], addr, rkey);
  if (ret == FI_SUCCESS) return LCI_OK;
  else if (ret == -FI_EAGAIN) return LCI_ERR_RETRY;
  else FI_SAFECALL(ret);
}

static inline LCI_error_t lc_server_put(LCID_server_t s, int rank, void* buf,
                                        size_t size, LCID_mr_t mr, uintptr_t base,
                                        uint32_t offset, LCID_rkey_t rkey,
                                        LCID_meta_t meta, void* ctx)
{
  LCIDI_server_t *server = (LCIDI_server_t*) s;
  uintptr_t addr;
  if (server->info->domain_attr->mr_mode & FI_MR_VIRT_ADDR || server->info->domain_attr->mr_mode & FI_MR_BASIC) {
    addr = base + offset;
  } else {
    addr = offset;
  }
  int ret = fi_writedata(server->ep, buf, size, fi_mr_desc((struct fid_mr*)mr), meta,
                         server->peer_addrs[rank], addr, rkey, ctx);
  if (ret == FI_SUCCESS) return LCI_OK;
  else if (ret == -FI_EAGAIN) return LCI_ERR_RETRY;
  else FI_SAFECALL(ret);
}

static inline int lc_server_recv_posted_num(LCID_server_t s) {
  LCIDI_server_t *server = (LCIDI_server_t*) s;
  return server->recv_posted;
}


#endif
