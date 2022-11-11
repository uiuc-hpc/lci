#ifndef SERVER_OFI_H_
#define SERVER_OFI_H_

#include <rdma/fabric.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_tagged.h>
#include <stdlib.h>
#include <string.h>
#include "dreg.h"

#include <rdma/fi_cm.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_rma.h>

#define FI_SAFECALL(x)                                                    \
  {                                                                       \
    int err = (x);                                                        \
    if (err < 0) err = -err;                                              \
    if (err) {                                                            \
      LCM_Assert(false, "err : %s (%s:%d)\n", fi_strerror(err), __FILE__, \
                 __LINE__);                                               \
    }                                                                     \
  }                                                                       \
  while (0)                                                               \
    ;

typedef struct LCISI_server_t {
  LCI_device_t device;
  struct fi_info* info;
  struct fid_fabric* fabric;
  struct fid_domain* domain;
  struct fid_ep* ep;
  struct fid_cq* cq;
  struct fid_av* av;
  fi_addr_t* peer_addrs;
} LCISI_server_t __attribute__((aligned(64)));

extern int g_next_rdma_key;

static inline void* LCISI_real_server_reg(LCIS_server_t s, void* buf,
                                          size_t size)
{
  LCISI_server_t* server = (LCISI_server_t*)s;
  struct fid_mr* mr;
  int rdma_key = __sync_fetch_and_add(&g_next_rdma_key, 1);
  FI_SAFECALL(fi_mr_reg(server->domain, buf, size,
                        FI_READ | FI_WRITE | FI_REMOTE_WRITE, 0, rdma_key, 0,
                        &mr, 0));
  if (server->info->domain_attr->mr_mode & FI_MR_ENDPOINT) {
    FI_SAFECALL(fi_mr_bind(mr, &server->ep->fid, 0));
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
  if (LCI_USE_DREG) {
    dreg_entry* entry = dreg_register(s, buf, size);
    LCM_DBG_Assert(entry != NULL, "Unable to register more memory!\n");
    mr.mr_p = entry;
    mr.address = (void*)(entry->pagenum << DREG_PAGEBITS);
    mr.length = entry->npages << DREG_PAGEBITS;
  } else {
    mr.mr_p = LCISI_real_server_reg(s, buf, size);
    mr.address = buf;
    mr.length = size;
  }
  return mr;
}

static inline void LCISD_rma_dereg(LCIS_mr_t mr)
{
  if (LCI_USE_DREG) {
    dreg_unregister((dreg_entry*)mr.mr_p);
  } else {
    LCISI_real_server_dereg(mr.mr_p);
  }
}

static inline LCIS_rkey_t LCISD_rma_rkey(LCIS_mr_t mr)
{
  if (LCI_USE_DREG) {
    return fi_mr_key((struct fid_mr*)(((dreg_entry*)mr.mr_p)->memhandle[0]));
  } else {
    return fi_mr_key((struct fid_mr*)(mr.mr_p));
  }
}

static inline void* ofi_rma_lkey(LCIS_mr_t mr)
{
  if (LCI_USE_DREG) {
    return fi_mr_desc((struct fid_mr*)(((dreg_entry*)mr.mr_p)->memhandle[0]));
  } else {
    return fi_mr_desc((struct fid_mr*)mr.mr_p);
  }
}

static inline int LCISD_poll_cq(LCIS_server_t s, LCIS_cq_entry_t* entry)
{
  LCISI_server_t* server = (LCISI_server_t*)s;
  struct fi_cq_data_entry fi_entry[LCI_CQ_MAX_POLL];
  struct fi_cq_err_entry error;
  ssize_t ne;
  int ret;

  ne = fi_cq_read(server->cq, &fi_entry, LCI_CQ_MAX_POLL);
  ret = ne;
  if (ne > 0) {
    // Got an entry here
    for (int i = 0; i < ne; i++) {
      if (fi_entry[i].flags & FI_RECV) {
        entry[i].opcode = LCII_OP_RECV;
        entry[i].ctx = fi_entry[i].op_context;
        entry[i].length = fi_entry[i].len;
        entry[i].imm_data = fi_entry[i].data & ((1ULL << 32) - 1);
        entry[i].rank = (int) (fi_entry[i].data >> 32);
      } else if (fi_entry[i].flags & FI_REMOTE_WRITE) {
        entry[i].opcode = LCII_OP_RDMA_WRITE;
        entry[i].ctx = NULL;
        entry[i].imm_data = fi_entry[i].data;
      } else {
        LCM_DBG_Assert(
            fi_entry[i].flags & FI_SEND || fi_entry[i].flags & FI_WRITE,
            "Unexpected OFI opcode!\n");
        entry[i].opcode = LCII_OP_SEND;
        entry[i].ctx = fi_entry[i].op_context;
      }
    }
  } else if (ne == -FI_EAGAIN) {
    ret = 0;
  } else {
    LCM_DBG_Assert(ne == -FI_EAVAIL, "unexpected return error: %s\n",
                   fi_strerror(-ne));
    fi_cq_readerr(server->cq, &error, 0);
    LCM_Assert(false, "Err %d: %s\n", error.err, fi_strerror(error.err));
  }
  return ret;
}

static inline void LCISD_post_recv(LCIS_server_t s, void* buf, uint32_t size,
                                   LCIS_mr_t mr, void* ctx)
{
  LCISI_server_t* server = (LCISI_server_t*)s;
  FI_SAFECALL(fi_recv(server->ep, buf, size, ofi_rma_lkey(mr), FI_ADDR_UNSPEC, ctx));
}

static inline LCI_error_t LCISD_post_sends(LCIS_server_t s, int rank, void* buf,
                                           size_t size, LCIS_meta_t meta)
{
  LCISI_server_t* server = (LCISI_server_t*)s;
  ssize_t ret = fi_injectdata(server->ep, buf, size, (uint64_t)LCI_RANK << 32 | meta,
                           server->peer_addrs[rank]);
  if (ret == FI_SUCCESS)
    return LCI_OK;
  else if (ret == -FI_EAGAIN)
    return LCI_ERR_RETRY;
  else
    FI_SAFECALL(ret);
}

static inline LCI_error_t LCISD_post_send(LCIS_server_t s, int rank, void* buf,
                                          size_t size, LCIS_mr_t mr,
                                          LCIS_meta_t meta, void* ctx)
{
  LCISI_server_t* server = (LCISI_server_t*)s;
  ssize_t ret = fi_senddata(server->ep, buf, size, ofi_rma_lkey(mr), (uint64_t)LCI_RANK << 32 | meta,
                         server->peer_addrs[rank], (struct fi_context*)ctx);
  if (ret == FI_SUCCESS)
    return LCI_OK;
  else if (ret == -FI_EAGAIN)
    return LCI_ERR_RETRY;
  else
    FI_SAFECALL(ret);
}

static inline LCI_error_t LCISD_post_puts(LCIS_server_t s, int rank, void* buf,
                                          size_t size, uintptr_t base,
                                          LCIS_offset_t offset,
                                          LCIS_rkey_t rkey)
{
  LCISI_server_t* server = (LCISI_server_t*)s;
  uintptr_t addr;
  if (server->info->domain_attr->mr_mode & FI_MR_VIRT_ADDR ||
      server->info->domain_attr->mr_mode & FI_MR_BASIC) {
    addr = base + offset;
  } else {
    addr = offset;
  }
  ssize_t ret = fi_inject_write(server->ep, buf, size, server->peer_addrs[rank],
                            addr, rkey);
  if (ret == FI_SUCCESS)
    return LCI_OK;
  else if (ret == -FI_EAGAIN)
    return LCI_ERR_RETRY;
  else
    FI_SAFECALL(ret);
}

static inline LCI_error_t LCISD_post_put(LCIS_server_t s, int rank, void* buf,
                                         size_t size, LCIS_mr_t mr,
                                         uintptr_t base, LCIS_offset_t offset,
                                         LCIS_rkey_t rkey, void* ctx)
{
  LCISI_server_t* server = (LCISI_server_t*)s;
  uintptr_t addr;
  if (server->info->domain_attr->mr_mode & FI_MR_VIRT_ADDR ||
      server->info->domain_attr->mr_mode & FI_MR_BASIC) {
    addr = base + offset;
  } else {
    addr = offset;
  }
  ssize_t ret = fi_write(server->ep, buf, size, ofi_rma_lkey(mr),
                     server->peer_addrs[rank], addr, rkey, ctx);
  if (ret == FI_SUCCESS)
    return LCI_OK;
  else if (ret == -FI_EAGAIN)
    return LCI_ERR_RETRY;
  else
    FI_SAFECALL(ret);
}

static inline LCI_error_t LCISD_post_putImms(LCIS_server_t s, int rank,
                                             void* buf, size_t size,
                                             uintptr_t base,
                                             LCIS_offset_t offset,
                                             LCIS_rkey_t rkey, uint32_t meta)
{
  LCISI_server_t* server = (LCISI_server_t*)s;
  uintptr_t addr;
  if (server->info->domain_attr->mr_mode & FI_MR_VIRT_ADDR ||
      server->info->domain_attr->mr_mode & FI_MR_BASIC) {
    addr = base + offset;
  } else {
    addr = offset;
  }
  ssize_t ret = fi_inject_writedata(server->ep, buf, size, meta,
                                server->peer_addrs[rank], addr, rkey);
  if (ret == FI_SUCCESS)
    return LCI_OK;
  else if (ret == -FI_EAGAIN)
    return LCI_ERR_RETRY;
  else
    FI_SAFECALL(ret);
}

static inline LCI_error_t LCISD_post_putImm(LCIS_server_t s, int rank,
                                            void* buf, size_t size,
                                            LCIS_mr_t mr, uintptr_t base,
                                            LCIS_offset_t offset,
                                            LCIS_rkey_t rkey, LCIS_meta_t meta,
                                            void* ctx)
{
  LCISI_server_t* server = (LCISI_server_t*)s;
  uintptr_t addr;
  if (server->info->domain_attr->mr_mode & FI_MR_VIRT_ADDR ||
      server->info->domain_attr->mr_mode & FI_MR_BASIC) {
    addr = base + offset;
  } else {
    addr = offset;
  }
  ssize_t ret = fi_writedata(server->ep, buf, size, ofi_rma_lkey(mr), meta,
                         server->peer_addrs[rank], addr, rkey, ctx);
  if (ret == FI_SUCCESS)
    return LCI_OK;
  else if (ret == -FI_EAGAIN)
    return LCI_ERR_RETRY;
  else
    FI_SAFECALL(ret);
}

#endif
