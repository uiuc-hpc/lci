#ifndef SERVER_OFI_H_
#define SERVER_OFI_H_

#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <rdma/fabric.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>

#include <rdma/fi_cm.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_rma.h>

#include "mv/lock.h"
#include "mv/macro.h"

#define SERVER_FI_DEBUG

#ifdef SERVER_FI_DEBUG
#define FI_SAFECALL(x)                                                    \
  {                                                                       \
    int err = (x);                                                        \
    if (err) {                                                            \
      printf("err : %s (%s:%d)\n", fi_strerror(err), __FILE__, __LINE__); \
      MPI_Abort(MPI_COMM_WORLD, err);                                     \
    }                                                                     \
  } while (0);

#else 
#define FI_SAFECALL(x) {(x);}
#endif

#define ALIGNMENT (4096)
#define ALIGNEDX(x) \
  (void*)((((uintptr_t)x + ALIGNMENT - 1) / ALIGNMENT * ALIGNMENT))
#define MAX_CQ_SIZE (16 * 1024)

typedef struct ofi_server {
  struct fi_info* fi;
  struct fid_fabric* fabric;
  struct fid_domain* domain;
  struct fid_ep* ep;
  struct fid_cq* scq;
  struct fid_cq* rcq;
  struct fid_mr* mr_heap;
  struct fid_mr* mr_sbuf;
  struct fid_av* av;
  fi_addr_t* fi_addr;
  uintptr_t* heap_addr;

  void* heap;
  void* sbuf;
  mv_pool* sbuf_pool;

  uint32_t heap_rkey;
  int recv_posted;
  mvh* mv;
  volatile int lock;
} ofi_server __attribute__((aligned(64)));

MV_INLINE void ofi_init(mvh* mv, size_t heap_size,
    ofi_server** s_ptr);
MV_INLINE void ofi_post_recv(ofi_server* s, mv_packet* p);
MV_INLINE int ofi_write_send(ofi_server* s, int rank, void* buf, size_t size,
                           void* ctx);
MV_INLINE void ofi_write_rma(ofi_server* s, int rank, void* from,
                          void* to, uint32_t rkey, size_t size, void* ctx);
MV_INLINE void ofi_write_rma_signal(ofi_server* s, int rank, void* from,
                                 void* to, uint32_t rkey,
                                 size_t size, uint32_t sid, void* ctx);
MV_INLINE void ofi_finalize(ofi_server* s);

MV_INLINE void ofi_init(mvh* mv, size_t heap_size,
    ofi_server** s_ptr)
{
// Create hint.
  struct fi_info* hints;
  hints = fi_allocinfo();
  hints->ep_attr->type = FI_EP_RDM;
  // hints->domain_attr->mr_mode = FI_MR_SCALABLE;
  hints->caps = FI_RMA | FI_MSG;
  hints->mode = FI_CONTEXT | FI_LOCAL_MR;

  ofi_server* s = malloc(sizeof(ofi_server));

  // Create info.
  FI_SAFECALL(fi_getinfo(FI_VERSION(1, 0), NULL, NULL, 0, hints, &s->fi));

  // Create libfabric obj.
  FI_SAFECALL(fi_fabric(s->fi->fabric_attr, &s->fabric, NULL));

  // Create domain.
  FI_SAFECALL(fi_domain(s->fabric, s->fi, &s->domain, NULL));

  // Create end-point;
  FI_SAFECALL(fi_endpoint(s->domain, s->fi, &s->ep, NULL));

  // Create cq.
  struct fi_cq_attr *cq_attr = malloc(sizeof(struct fi_cq_attr));
  memset(cq_attr, 0, sizeof(cq_attr));
  cq_attr->format = FI_CQ_FORMAT_DATA;
  cq_attr->size = MAX_CQ_SIZE;
  FI_SAFECALL(fi_cq_open(s->domain, cq_attr, &s->scq, NULL));
  FI_SAFECALL(fi_cq_open(s->domain, cq_attr, &s->rcq, NULL));

  // Bind my ep to cq.
  FI_SAFECALL(fi_ep_bind(s->ep, (fid_t)s->scq, FI_SEND | FI_TRANSMIT));
  FI_SAFECALL(fi_ep_bind(s->ep, (fid_t)s->rcq, FI_RECV));

  // Get memory for heap.
  s->heap = 0 ;//std::move(unique_ptr<char[]>(new char[heap_size]));
  posix_memalign(&s->heap, 4096, heap_size);

  FI_SAFECALL(fi_mr_reg(s->domain, s->heap, heap_size,
                        FI_READ | FI_WRITE | FI_REMOTE_WRITE | FI_REMOTE_READ,
                        0, 0, 0, &s->mr_heap, 0));

  s->heap_rkey = fi_mr_key(s->mr_heap);

  // s->sbuf = std::move(
      // unique_ptr<char[]>(new char[sizeof(mv_packet) * (MAX_SEND + MAX_RECV + 2)]));
  s->sbuf = 0;
  posix_memalign(&s->sbuf, 4096, (MAX_SEND + MAX_RECV) * sizeof(mv_packet));

  FI_SAFECALL(fi_mr_reg(s->domain, s->sbuf,
                        sizeof(mv_packet) * (MAX_SEND + MAX_RECV + 2),
                        FI_READ | FI_WRITE | FI_REMOTE_WRITE | FI_REMOTE_READ,
                        0, 1, 0, &s->mr_sbuf, 0));

  // s->sbuf_alloc = std::move(
      // std::unique_ptr<pinned_pool>(new pinned_pool(ALIGNEDX(s->sbuf.get()))));
  mv_pool_create(&s->sbuf_pool, s->sbuf, sizeof(mv_packet), MAX_SEND + MAX_RECV);

  MPI_Comm_rank(MPI_COMM_WORLD, &mv->me);
  MPI_Comm_size(MPI_COMM_WORLD, &mv->size);

  // Now exchange end-point address and heap address.
  size_t addrlen = 0;
  fi_getname((fid_t)s->ep, NULL, &addrlen);
  void* addr = malloc(addrlen + sizeof(uintptr_t));
  FI_SAFECALL(fi_getname((fid_t)s->ep, addr, &addrlen));

  // Set heap address at the end. TODO(danghvu): Need a more generic way.
  uintptr_t myaddr = (uintptr_t)s->heap;
  *(uintptr_t*)((char*)addr + addrlen) = myaddr;

  s->fi_addr = malloc(sizeof(fi_addr_t) * mv->size);
  s->heap_addr = malloc(sizeof(uintptr_t) * mv->size);

  struct fi_av_attr av_attr;
  memset(&av_attr, 0, sizeof(av_attr));
  av_attr.type = FI_AV_MAP;
  FI_SAFECALL(fi_av_open(s->domain, &av_attr, &s->av, NULL));
  FI_SAFECALL(fi_ep_bind(s->ep, (fid_t)s->av, 0));
  FI_SAFECALL(fi_enable(s->ep));

  for (int i = 0; i < mv->size; i++) {
    if (i != mv->me) {
      void* destaddr = malloc(addrlen + sizeof(uintptr_t));
      MPI_Sendrecv(addr, addrlen + sizeof(uintptr_t), MPI_BYTE, i, 99, destaddr,
                   addrlen + sizeof(uintptr_t), MPI_BYTE, i, 99, MPI_COMM_WORLD,
                   MPI_STATUS_IGNORE);
      if (fi_av_insert(s->av, destaddr, 1, &s->fi_addr[i], 0, NULL) == -1) {
        MPI_Abort(MPI_COMM_WORLD, -1);
      }
      s->heap_addr[i] = *(uintptr_t*)((char*)destaddr + addrlen);
    }
  }

  s->recv_posted = 0;
  s->mv = mv;
  mv->pkpool = s->sbuf_pool;
  *s_ptr = s;
}

MV_INLINE int ofi_progress(ofi_server* s)
{  // profiler& p, long long& r, long long &s) {
  struct fi_cq_data_entry entry;
  struct fi_cq_err_entry error;
  ssize_t ret;
  int rett = 0;

  ret = fi_cq_read(s->rcq, &entry, 1);
  if (ret > 0) {
    // Got an entry here ?
    s->recv_posted--;
    if (entry.flags & FI_REMOTE_CQ_DATA) {
      mv_serve_imm(entry.data);
    } else {
      mv_serve_recv(s->mv, (mv_packet*)entry.op_context);
    }
    rett = 1;
  } else if (ret == -FI_EAGAIN) {
  } else if (ret == -FI_EAVAIL) {
    fi_cq_readerr(s->rcq, &error, 0);
    printf("Err: %s\n", fi_strerror(error.err));
    MPI_Abort(MPI_COMM_WORLD, error.err);
  } else if (ret < 0) {
    /* handle error */
    printf("Err: %s\n", fi_strerror(ret));
    MPI_Abort(MPI_COMM_WORLD, error.err);
  }

  ret = fi_cq_read(s->scq, &entry, 1);
  if (ret > 0) {
    // Got an entry here ?
    mv_serve_send(s->mv, (mv_packet*)entry.op_context);
    rett = 1;
  } else if (ret == -FI_EAGAIN) {
  } else if (ret == -FI_EAVAIL) {
    fi_cq_readerr(s->scq, &error, 0);
    printf("Err: %s\n", fi_strerror(error.err));
    MPI_Abort(MPI_COMM_WORLD, error.err);
  } else if (ret < 0) {
    /* handle error */
    printf("Err: %s\n", fi_strerror(ret));
    MPI_Abort(MPI_COMM_WORLD, error.err);
  }

  if (s->recv_posted < MAX_RECV) ofi_post_recv(s, mv_pool_get(s->sbuf_pool));

  return rett;
}

MV_INLINE void ofi_post_recv(ofi_server* s, mv_packet* p)
{
  if (p == NULL) return;
  mv_spin_lock(&s->lock);
  FI_SAFECALL(
      fi_recv(s->ep, &p->data, sizeof(mv_packet_data), 0, FI_ADDR_UNSPEC, &p->context));
  s->recv_posted++;
  mv_spin_unlock(&s->lock);
}

MV_INLINE int ofi_write_send(ofi_server* s, int rank, void* buf, size_t size,
                             void* ctx)
{
  //FIXME(danghvu): should take from the device.
  if (size >= 30) {
    mv_spin_lock(&s->lock);
    FI_SAFECALL(
        fi_send(s->ep, buf, size, 0, s->fi_addr[rank], (struct fi_context*) ctx));
    mv_spin_unlock(&s->lock);
    return 1;
  } else {
    mv_spin_lock(&s->lock);
    FI_SAFECALL(fi_inject(s->ep, buf, size, s->fi_addr[rank]));
    mv_pool_put(s->sbuf_pool, (mv_packet*)ctx);
    mv_spin_unlock(&s->lock);
    return 0;
  }
}

MV_INLINE void ofi_write_rma(ofi_server* s, int rank, void* from, void* to,
                          uint32_t rkey, size_t size, void* ctx)
{
  mv_spin_lock(&s->lock);
  FI_SAFECALL(fi_write(s->ep, from, size, 0, s->fi_addr[rank],
                       (uintptr_t)to - s->heap_addr[rank],  // this is offset.
                       rkey, ctx));
  mv_spin_unlock(&s->lock);
}

MV_INLINE void ofi_write_rma_signal(ofi_server* s, int rank, void* from, void* to,
                                 uint32_t rkey, size_t size, uint32_t sid,
                                 void* ctx)
{
  mv_spin_lock(&s->lock);
  FI_SAFECALL(
      fi_writedata(s->ep, from, size, 0, sid, s->fi_addr[rank],
                   (uintptr_t)to - s->heap_addr[rank],  // this is offset.
                   rkey, ctx));
  mv_spin_unlock(&s->lock);
}

MV_INLINE void ofi_finalize(ofi_server* s)
{
  free(s);
}

MV_INLINE uint32_t ofi_heap_rkey(ofi_server* s, int node __UNUSED__) {
  return s->heap_rkey;
}

MV_INLINE void* ofi_heap_ptr(ofi_server* s) { return ALIGNEDX(s->heap); }

#define mv_server_init ofi_init
#define mv_server_send ofi_write_send
#define mv_server_rma ofi_write_rma
#define mv_server_rma_signal ofi_write_rma_signal
#define mv_server_heap_rkey ofi_heap_rkey
#define mv_server_heap_ptr ofi_heap_ptr
#define mv_server_progress ofi_progress
#define mv_server_finalize ofi_finalize

#endif
