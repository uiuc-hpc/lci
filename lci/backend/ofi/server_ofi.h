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

#define FI_SAFECALL(x)                                                        \
  {                                                                           \
    int err = (x);                                                            \
    if (err < 0) err = -err;                                                  \
    if (err) {                                                                \
      LCI_DBG_Assert(false, "err : %s (%s:%d)\n", fi_strerror(err), __FILE__, \
                     __LINE__);                                               \
    }                                                                         \
  }                                                                           \
  while (0)                                                                   \
    ;

#define LCISI_OFI_CS_TRY_ENTER(endpoint_p, mode, ret)                        \
  if (LCI_BACKEND_TRY_LOCK_MODE & mode && !endpoint_p->is_single_threaded && \
      !LCIU_try_acquire_spinlock(&endpoint_p->lock))                         \
    return ret;

#define LCISI_OFI_CS_EXIT(endpoint_p, mode)                                \
  if (LCI_BACKEND_TRY_LOCK_MODE & mode && !endpoint_p->is_single_threaded) \
    LCIU_release_spinlock(&endpoint_p->lock);

struct LCISI_endpoint_t;

typedef struct __attribute__((aligned(LCI_CACHE_LINE))) LCISI_server_t {
  LCI_device_t device;
  struct fi_info* info;
  struct fid_fabric* fabric;
  struct fid_domain* domain;
  struct LCISI_endpoint_t* endpoints[LCI_SERVER_MAX_ENDPOINTS];
  int endpoint_count;
  bool cxi_mr_bind_hack;
} LCISI_server_t;

typedef struct __attribute__((aligned(LCI_CACHE_LINE))) LCISI_endpoint_t {
  LCISI_server_t* server;
  struct fid_ep* ep;
  struct fid_cq* cq;
  struct fid_av* av;
  fi_addr_t* peer_addrs;
  bool is_single_threaded;
  LCIU_spinlock_t lock;
} LCISI_endpoint_t;

extern int g_next_rdma_key;

static inline void* LCISI_real_server_reg(LCIS_server_t s, void* buf,
                                          size_t size)
{
  LCISI_server_t* server = (LCISI_server_t*)s;
  int rdma_key;
  if (server->info->domain_attr->mr_mode & FI_MR_PROV_KEY) {
    rdma_key = 0;
  } else {
    rdma_key = __sync_fetch_and_add(&g_next_rdma_key, 1);
  }
  struct fid_mr* mr;
  FI_SAFECALL(fi_mr_reg(server->domain, buf, size,
                        FI_READ | FI_WRITE | FI_REMOTE_WRITE, 0, rdma_key, 0,
                        &mr, 0));
  if (server->info->domain_attr->mr_mode & FI_MR_ENDPOINT) {
    LCI_DBG_Assert(server->endpoint_count >= 1, "No endpoints available!\n");
    if (server->cxi_mr_bind_hack) {
      // A temporary fix for the cxi provider, currently cxi cannot bind a
      // memory region to more than one endpoint.
      FI_SAFECALL(fi_mr_bind(
          mr, &server->endpoints[server->endpoint_count - 1]->ep->fid, 0));
    } else {
      for (int i = 0; i < server->endpoint_count; ++i) {
        FI_SAFECALL(fi_mr_bind(mr, &server->endpoints[i]->ep->fid, 0));
      }
    }
    FI_SAFECALL(fi_mr_enable(mr));
  }
  return (void*)mr;
}

static inline void LCISI_real_server_dereg(void* mr_opaque)
{
  struct fid_mr* mr = (struct fid_mr*)mr_opaque;
  FI_SAFECALL(fi_close((struct fid*)&mr->fid));
}

static inline LCIS_mr_t LCISD_rma_reg(LCIS_server_t s, void* buf, size_t size)
{
  LCIS_mr_t mr;
  mr.mr_p = LCISI_real_server_reg(s, buf, size);
  mr.address = buf;
  mr.length = size;
  return mr;
}

static inline void LCISD_rma_dereg(LCIS_mr_t mr)
{
  LCISI_real_server_dereg(mr.mr_p);
}

static inline LCIS_rkey_t LCISD_rma_rkey(LCIS_mr_t mr)
{
  return fi_mr_key((struct fid_mr*)(mr.mr_p));
}

static inline void* ofi_rma_lkey(LCIS_mr_t mr)
{
  return fi_mr_desc((struct fid_mr*)mr.mr_p);
}

static inline int LCISD_poll_cq(LCIS_endpoint_t endpoint_pp,
                                LCIS_cq_entry_t* entry)
{
  LCISI_endpoint_t* endpoint_p = (LCISI_endpoint_t*)endpoint_pp;
  struct fi_cq_data_entry fi_entry[LCI_CQ_MAX_POLL];
  struct fi_cq_err_entry error;
  ssize_t ne;
  int ret;

  LCISI_OFI_CS_TRY_ENTER(endpoint_p, LCI_BACKEND_TRY_LOCK_POLL, 0)
  ne = fi_cq_read(endpoint_p->cq, &fi_entry, LCI_CQ_MAX_POLL);
  LCISI_OFI_CS_EXIT(endpoint_p, LCI_BACKEND_TRY_LOCK_POLL)
  ret = ne;
  if (ne > 0) {
    // Got an entry here
    for (int i = 0; i < ne; i++) {
      if (fi_entry[i].flags & FI_RECV) {
        entry[i].opcode = LCII_OP_RECV;
        entry[i].ctx = fi_entry[i].op_context;
        entry[i].length = fi_entry[i].len;
        entry[i].imm_data = fi_entry[i].data & ((1ULL << 32) - 1);
        entry[i].rank = (int)(fi_entry[i].data >> 32);
      } else if (fi_entry[i].flags & FI_REMOTE_WRITE) {
        entry[i].opcode = LCII_OP_RDMA_WRITE;
        entry[i].ctx = NULL;
        entry[i].imm_data = fi_entry[i].data;
      } else {
        LCI_DBG_Assert(
            fi_entry[i].flags & FI_SEND || fi_entry[i].flags & FI_WRITE,
            "Unexpected OFI opcode!\n");
        entry[i].opcode = LCII_OP_SEND;
        entry[i].ctx = fi_entry[i].op_context;
      }
    }
  } else if (ne == -FI_EAGAIN) {
    ret = 0;
  } else {
    LCI_DBG_Assert(ne == -FI_EAVAIL, "unexpected return error: %s\n",
                   fi_strerror(-ne));
    fi_cq_readerr(endpoint_p->cq, &error, 0);
    LCI_Assert(false, "Err %d: %s\n", error.err, fi_strerror(error.err));
  }
  return ret;
}

static inline LCI_error_t LCISD_post_recv(LCIS_endpoint_t endpoint_pp,
                                          void* buf, uint32_t size,
                                          LCIS_mr_t mr, void* ctx)
{
  LCISI_endpoint_t* endpoint_p = (LCISI_endpoint_t*)endpoint_pp;

  LCISI_OFI_CS_TRY_ENTER(endpoint_p, LCI_BACKEND_TRY_LOCK_RECV,
                         LCI_ERR_RETRY_LOCK)
  ssize_t ret =
      fi_recv(endpoint_p->ep, buf, size, ofi_rma_lkey(mr), FI_ADDR_UNSPEC, ctx);
  LCISI_OFI_CS_EXIT(endpoint_p, LCI_BACKEND_TRY_LOCK_RECV)
  if (ret == FI_SUCCESS)
    return LCI_OK;
  else if (ret == -FI_EAGAIN)
    return LCI_ERR_RETRY;
  else {
    FI_SAFECALL(ret);
    return LCI_ERR_FATAL;
  }
}

static inline LCI_error_t LCISD_post_sends(LCIS_endpoint_t endpoint_pp,
                                           int rank, void* buf, size_t size,
                                           LCIS_meta_t meta)
{
  LCISI_endpoint_t* endpoint_p = (LCISI_endpoint_t*)endpoint_pp;
  LCISI_OFI_CS_TRY_ENTER(endpoint_p, LCI_BACKEND_TRY_LOCK_SEND,
                         LCI_ERR_RETRY_LOCK)
  ssize_t ret =
      fi_injectdata(endpoint_p->ep, buf, size, (uint64_t)LCI_RANK << 32 | meta,
                    endpoint_p->peer_addrs[rank]);
  LCISI_OFI_CS_EXIT(endpoint_p, LCI_BACKEND_TRY_LOCK_SEND)
  if (ret == FI_SUCCESS)
    return LCI_OK;
  else if (ret == -FI_EAGAIN)
    return LCI_ERR_RETRY_NOMEM;
  else {
    FI_SAFECALL(ret);
    return LCI_ERR_FATAL;
  }
}

static inline LCI_error_t LCISD_post_send(LCIS_endpoint_t endpoint_pp, int rank,
                                          void* buf, size_t size, LCIS_mr_t mr,
                                          LCIS_meta_t meta, void* ctx)
{
  LCISI_endpoint_t* endpoint_p = (LCISI_endpoint_t*)endpoint_pp;
  LCISI_OFI_CS_TRY_ENTER(endpoint_p, LCI_BACKEND_TRY_LOCK_SEND,
                         LCI_ERR_RETRY_LOCK)
  ssize_t ret =
      fi_senddata(endpoint_p->ep, buf, size, ofi_rma_lkey(mr),
                  (uint64_t)LCI_RANK << 32 | meta, endpoint_p->peer_addrs[rank],
                  (struct fi_context*)ctx);
  LCISI_OFI_CS_EXIT(endpoint_p, LCI_BACKEND_TRY_LOCK_SEND)
  if (ret == FI_SUCCESS)
    return LCI_OK;
  else if (ret == -FI_EAGAIN)
    return LCI_ERR_RETRY_NOMEM;
  else {
    FI_SAFECALL(ret);
    return LCI_ERR_FATAL;
  }
}

static inline LCI_error_t LCISD_post_puts(LCIS_endpoint_t endpoint_pp, int rank,
                                          void* buf, size_t size,
                                          uintptr_t base, LCIS_offset_t offset,
                                          LCIS_rkey_t rkey)
{
  LCISI_endpoint_t* endpoint_p = (LCISI_endpoint_t*)endpoint_pp;
  LCI_Assert(
      !endpoint_p->server->cxi_mr_bind_hack ||
          endpoint_p == endpoint_p->server
                            ->endpoints[endpoint_p->server->endpoint_count - 1],
      "We are using cxi mr_bind hacking mode but unexpected endpoint is "
      "performing remote put. Try `export LCI_ENABLE_PRG_NET_ENDPOINT=0`.\n");
  uintptr_t addr;
  if (endpoint_p->server->info->domain_attr->mr_mode & FI_MR_VIRT_ADDR ||
      endpoint_p->server->info->domain_attr->mr_mode & FI_MR_BASIC) {
    addr = base + offset;
  } else {
    addr = offset;
  }
  LCISI_OFI_CS_TRY_ENTER(endpoint_p, LCI_BACKEND_TRY_LOCK_SEND,
                         LCI_ERR_RETRY_LOCK)
  ssize_t ret = fi_inject_write(endpoint_p->ep, buf, size,
                                endpoint_p->peer_addrs[rank], addr, rkey);
  LCISI_OFI_CS_EXIT(endpoint_p, LCI_BACKEND_TRY_LOCK_SEND)
  if (ret == FI_SUCCESS)
    return LCI_OK;
  else if (ret == -FI_EAGAIN)
    return LCI_ERR_RETRY_NOMEM;
  else {
    FI_SAFECALL(ret);
    return LCI_ERR_FATAL;
  }
}

static inline LCI_error_t LCISD_post_put(LCIS_endpoint_t endpoint_pp, int rank,
                                         void* buf, size_t size, LCIS_mr_t mr,
                                         uintptr_t base, LCIS_offset_t offset,
                                         LCIS_rkey_t rkey, void* ctx)
{
  LCISI_endpoint_t* endpoint_p = (LCISI_endpoint_t*)endpoint_pp;
  LCI_Assert(
      !endpoint_p->server->cxi_mr_bind_hack ||
          endpoint_p == endpoint_p->server
                            ->endpoints[endpoint_p->server->endpoint_count - 1],
      "We are using cxi mr_bind hacking mode but an unexpected endpoint is "
      "performing remote put. Try `export LCI_ENABLE_PRG_NET_ENDPOINT=0`.\n");
  uintptr_t addr;
  if (endpoint_p->server->info->domain_attr->mr_mode & FI_MR_VIRT_ADDR ||
      endpoint_p->server->info->domain_attr->mr_mode & FI_MR_BASIC) {
    addr = base + offset;
  } else {
    addr = offset;
  }
  LCISI_OFI_CS_TRY_ENTER(endpoint_p, LCI_BACKEND_TRY_LOCK_SEND,
                         LCI_ERR_RETRY_LOCK)
  ssize_t ret = fi_write(endpoint_p->ep, buf, size, ofi_rma_lkey(mr),
                         endpoint_p->peer_addrs[rank], addr, rkey, ctx);
  LCISI_OFI_CS_EXIT(endpoint_p, LCI_BACKEND_TRY_LOCK_SEND)
  if (ret == FI_SUCCESS)
    return LCI_OK;
  else if (ret == -FI_EAGAIN)
    return LCI_ERR_RETRY_NOMEM;
  else {
    FI_SAFECALL(ret);
    return LCI_ERR_FATAL;
  }
}

static inline LCI_error_t LCISD_post_putImms(LCIS_endpoint_t endpoint_pp,
                                             int rank, void* buf, size_t size,
                                             uintptr_t base,
                                             LCIS_offset_t offset,
                                             LCIS_rkey_t rkey, uint32_t meta)
{
  LCISI_endpoint_t* endpoint_p = (LCISI_endpoint_t*)endpoint_pp;
  LCI_Assert(
      !endpoint_p->server->cxi_mr_bind_hack ||
          endpoint_p == endpoint_p->server
                            ->endpoints[endpoint_p->server->endpoint_count - 1],
      "We are using cxi mr_bind hacking mode but an unexpected endpoint is "
      "performing remote put. Try `export LCI_ENABLE_PRG_NET_ENDPOINT=0`.\n");
  uintptr_t addr;
  if (endpoint_p->server->info->domain_attr->mr_mode & FI_MR_VIRT_ADDR ||
      endpoint_p->server->info->domain_attr->mr_mode & FI_MR_BASIC) {
    addr = base + offset;
  } else {
    addr = offset;
  }
  LCISI_OFI_CS_TRY_ENTER(endpoint_p, LCI_BACKEND_TRY_LOCK_SEND,
                         LCI_ERR_RETRY_LOCK)
  ssize_t ret = fi_inject_writedata(endpoint_p->ep, buf, size, meta,
                                    endpoint_p->peer_addrs[rank], addr, rkey);
  LCISI_OFI_CS_EXIT(endpoint_p, LCI_BACKEND_TRY_LOCK_SEND)
  if (ret == FI_SUCCESS)
    return LCI_OK;
  else if (ret == -FI_EAGAIN)
    return LCI_ERR_RETRY_NOMEM;
  else {
    FI_SAFECALL(ret);
    return LCI_ERR_FATAL;
  }
}

static inline LCI_error_t LCISD_post_putImm(LCIS_endpoint_t endpoint_pp,
                                            int rank, void* buf, size_t size,
                                            LCIS_mr_t mr, uintptr_t base,
                                            LCIS_offset_t offset,
                                            LCIS_rkey_t rkey, LCIS_meta_t meta,
                                            void* ctx)
{
  LCISI_endpoint_t* endpoint_p = (LCISI_endpoint_t*)endpoint_pp;
  LCI_Assert(
      !endpoint_p->server->cxi_mr_bind_hack ||
          endpoint_p == endpoint_p->server
                            ->endpoints[endpoint_p->server->endpoint_count - 1],
      "We are using cxi mr_bind hacking mode but an unexpected endpoint is "
      "performing remote put. Try `export LCI_ENABLE_PRG_NET_ENDPOINT=0`.\n");
  uintptr_t addr;
  if (endpoint_p->server->info->domain_attr->mr_mode & FI_MR_VIRT_ADDR ||
      endpoint_p->server->info->domain_attr->mr_mode & FI_MR_BASIC) {
    addr = base + offset;
  } else {
    addr = offset;
  }
  LCISI_OFI_CS_TRY_ENTER(endpoint_p, LCI_BACKEND_TRY_LOCK_SEND,
                         LCI_ERR_RETRY_LOCK)
  ssize_t ret = fi_writedata(endpoint_p->ep, buf, size, ofi_rma_lkey(mr), meta,
                             endpoint_p->peer_addrs[rank], addr, rkey, ctx);
  LCISI_OFI_CS_EXIT(endpoint_p, LCI_BACKEND_TRY_LOCK_SEND)
  if (ret == FI_SUCCESS)
    return LCI_OK;
  else if (ret == -FI_EAGAIN)
    return LCI_ERR_RETRY_NOMEM;
  else {
    FI_SAFECALL(ret);
    return LCI_ERR_FATAL;
  }
}

#endif
