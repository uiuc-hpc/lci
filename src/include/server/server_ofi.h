#ifndef SERVER_OFI_H_
#define SERVER_OFI_H_

#include <cstdlib>
#include <cstring>
#include <mpi.h>
#include <rdma/fabric.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_tagged.h>

#include <rdma/fi_cm.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_rma.h>

#include "lock.h"

#include <vector>
using std::vector;

#define FI_SAFECALL(x)                                                    \
  {                                                                       \
    int err = (x);                                                        \
    if (err) {                                                            \
      printf("err : %s (%s:%d)\n", fi_strerror(err), __FILE__, __LINE__); \
      MPI_Abort(MPI_COMM_WORLD, err);                                     \
    }                                                                     \
  }

#define ALIGNMENT (4096)
#define ALIGNEDX(x) \
  (void*)((((uintptr_t)x + ALIGNMENT - 1) / ALIGNMENT * ALIGNMENT))
#define MAX_CQ_SIZE (16 * 1024)

struct ofi_server {
  std::thread poll_thread;
  volatile bool stop;

  fi_info* fi;
  fid_fabric* fabric;
  fid_domain* domain;
  fid_ep* ep;
  fid_cq* scq;
  fid_cq* rcq;
  fid_mr* mr_heap;
  fid_mr* mr_sbuf;
  fid_av* av;
  vector<fi_addr_t> fi_addr;
  vector<uintptr_t> heap_addr;

  unique_ptr<char[]> heap;
  unique_ptr<char[]> sbuf;

  uint32_t heap_rkey;
  int recv_posted;
  unique_ptr<pinned_pool> sbuf_alloc;
  mv_engine* mv;
  mv_pp* pkpool;
  volatile int lock;
} __attribute__((aligned(64)));

inline void ofi_init(mv_engine* mv, mv_pp*, size_t heap_size,
                     ofi_server** s_ptr);
inline void ofi_post_recv(ofi_server* s, packet* p);
inline void ofi_serve(ofi_server* s);
inline void ofi_write_send(ofi_server* s, int rank, void* buf, size_t size,
                           void* ctx);
inline void ofi_write_rma(ofi_server* s, int rank, void* from, uint32_t lkey,
                          void* to, uint32_t rkey, size_t size, void* ctx);
inline void ofi_write_rma_signal(ofi_server* s, int rank, void* from,
                                 uint32_t lkey, void* to, uint32_t rkey,
                                 size_t size, uint32_t sid, void* ctx);
inline void ofi_finalize(ofi_server* s);

inline void ofi_init(mv_engine* mv, mv_pp* pkpool, size_t heap_size,
                     ofi_server** s_ptr)
{
#ifdef USE_AFFI
  affinity::set_me_to(0);
#endif

// Create hint.
#if 1
  struct fi_info* hints;
  hints = fi_allocinfo();
  hints->ep_attr->type = FI_EP_RDM;
  // hints->domain_attr->mr_mode = FI_MR_SCALABLE;
  hints->caps = FI_RMA | FI_MSG;
  hints->mode = FI_CONTEXT | FI_LOCAL_MR;
#endif

  ofi_server* s = new ofi_server();

  // Create info.
  FI_SAFECALL(fi_getinfo(FI_VERSION(1, 0), NULL, NULL, 0, hints, &s->fi));

  // Create libfabric obj.
  FI_SAFECALL(fi_fabric(s->fi->fabric_attr, &s->fabric, NULL));

  // Create domain.
  FI_SAFECALL(fi_domain(s->fabric, s->fi, &s->domain, NULL));

  // Create end-point;
  FI_SAFECALL(fi_endpoint(s->domain, s->fi, &s->ep, NULL));

  // Create cq.
  struct fi_cq_attr cq_attr;
  memset(&cq_attr, 0, sizeof(cq_attr));
  cq_attr.format = FI_CQ_FORMAT_DATA;
  cq_attr.size = MAX_CQ_SIZE;
  FI_SAFECALL(fi_cq_open(s->domain, &cq_attr, &s->scq, NULL));
  FI_SAFECALL(fi_cq_open(s->domain, &cq_attr, &s->rcq, NULL));

  // Bind my ep to cq.
  FI_SAFECALL(fi_ep_bind(s->ep, (fid_t)s->scq, FI_SEND | FI_TRANSMIT));
  FI_SAFECALL(fi_ep_bind(s->ep, (fid_t)s->rcq, FI_RECV));

  // Get memory for heap.
  s->heap = std::move(unique_ptr<char[]>(new char[heap_size]));

  FI_SAFECALL(fi_mr_reg(s->domain, s->heap.get(), heap_size,
                        FI_READ | FI_WRITE | FI_REMOTE_WRITE | FI_REMOTE_READ,
                        0, 0, 0, &s->mr_heap, 0));

  s->heap_rkey = fi_mr_key(s->mr_heap);

  s->sbuf = std::move(
      unique_ptr<char[]>(new char[sizeof(packet) * (MAX_SEND + MAX_RECV + 2)]));

  FI_SAFECALL(fi_mr_reg(s->domain, s->sbuf.get(),
                        sizeof(packet) * (MAX_SEND + MAX_RECV + 2),
                        FI_READ | FI_WRITE | FI_REMOTE_WRITE | FI_REMOTE_READ,
                        0, 1, 0, &s->mr_sbuf, 0));

  s->sbuf_alloc = std::move(
      std::unique_ptr<pinned_pool>(new pinned_pool(ALIGNEDX(s->sbuf.get()))));

  MPI_Comm_rank(MPI_COMM_WORLD, &mv->me);
  MPI_Comm_size(MPI_COMM_WORLD, &mv->size);

  // Now exchange end-point address and heap address.
  size_t addrlen = 0;
  fi_getname((fid_t)s->ep, NULL, &addrlen);
  void* addr = malloc(addrlen + sizeof(uintptr_t));
  FI_SAFECALL(fi_getname((fid_t)s->ep, addr, &addrlen));

  // Set heap address at the end. TODO(danghvu): Need a more generic way.
  uintptr_t myaddr = (uintptr_t)s->heap.get();
  *(uintptr_t*)((char*)addr + addrlen) = myaddr;

  s->fi_addr.resize(mv->size);
  s->heap_addr.resize(mv->size);

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

  // Prepare the packet_mgr and prepost some packet.
  for (int i = 0; i < MAX_SEND + MAX_RECV; i++) {
    mv_pp_free(pkpool, (packet*)s->sbuf_alloc->allocate());
  }
  s->recv_posted = 0;
  s->pkpool = pkpool;
  s->mv = mv;
  *s_ptr = s;
}

struct my_context {
  my_context(void* ctx) : ctx_(ctx) {}
  struct fi_context fi_ctx;
  void* ctx_;
};

MV_INLINE bool ofi_progress(ofi_server* s)
{  // profiler& p, long long& r, long long &s) {
  initt(t);
  startt(t);

  struct fi_cq_data_entry entry;
  struct fi_cq_err_entry error;
  ssize_t ret;
  bool rett = false;

  ret = fi_cq_read(s->rcq, &entry, 1);
  if (ret > 0) {
    // Got an entry here ?
    s->recv_posted--;
    if (entry.flags & FI_REMOTE_CQ_DATA) {
      mv_serve_imm(entry.data);
    } else {
      mv_serve_recv(s->mv, (packet*)(((my_context*)entry.op_context)->ctx_));
      mv_spin_lock(&s->lock);
      delete (my_context*)entry.op_context;
      mv_spin_unlock(&s->lock);
    }
    rett = true;
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
    mv_serve_send(s->mv, (packet*)(((my_context*)entry.op_context)->ctx_));
    mv_spin_lock(&s->lock);
    delete (my_context*)entry.op_context;
    mv_spin_unlock(&s->lock);
    rett = true;
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

  if (s->recv_posted < MAX_RECV) ofi_post_recv(s, mv_pp_alloc_nb(s->pkpool, 0));

  stopt(t) return rett;
}

void ofi_serve(ofi_server* s)
{
  s->poll_thread = std::thread([s] {
#ifdef USE_AFFI
    affinity::set_me_to_last();
#endif

#ifdef USE_PAPI
    profiler server({PAPI_L1_DCM});
    server.start();
#endif

    while (unlikely(!s->stop)) {
      while (ofi_progress(s)) {
      };
    }

#ifdef USE_PAPI
    server.stop();
    server.print();
#endif
  });
}

inline void ofi_post_recv(ofi_server* s, packet* p)
{
  if (p == NULL) return;
  mv_spin_lock(&s->lock);
  FI_SAFECALL(
      fi_recv(s->ep, p, sizeof(packet), 0, FI_ADDR_UNSPEC, new my_context(p)));
  s->recv_posted++;
  mv_spin_unlock(&s->lock);
}

inline void ofi_write_send(ofi_server* s, int rank, void* buf, size_t size,
                           void* ctx)
{
  mv_spin_lock(&s->lock);
  if (size > 32) {
    FI_SAFECALL(
        fi_send(s->ep, buf, size, 0, s->fi_addr[rank], new my_context(ctx)));
  } else {
    FI_SAFECALL(fi_inject(s->ep, buf, size, s->fi_addr[rank]));
    mv_pp_free_to(s->pkpool, (packet*)ctx, ((packet*)ctx)->header.poolid);
  }
  mv_spin_unlock(&s->lock);
}

inline void ofi_write_rma(ofi_server* s, int rank, void* from, void* to,
                          uint32_t rkey, size_t size, void* ctx)
{
  mv_spin_lock(&s->lock);
  FI_SAFECALL(fi_write(s->ep, from, size, 0, s->fi_addr[rank],
                       (uintptr_t)to - s->heap_addr[rank],  // this is offset.
                       rkey, new my_context(ctx)));
  mv_spin_unlock(&s->lock);
}

inline void ofi_write_rma_signal(ofi_server* s, int rank, void* from, void* to,
                                 uint32_t rkey, size_t size, uint32_t sid,
                                 void* ctx)
{
  mv_spin_lock(&s->lock);
  FI_SAFECALL(
      fi_writedata(s->ep, from, size, 0, sid, s->fi_addr[rank],
                   (uintptr_t)to - s->heap_addr[rank],  // this is offset.
                   rkey, new my_context(ctx)));
  mv_spin_unlock(&s->lock);
}

inline void ofi_finalize(ofi_server* s)
{
  s->stop = true;
  s->poll_thread.join();
}

inline uint32_t ofi_heap_rkey(ofi_server* s) { return s->heap_rkey; }
inline void* ofi_heap_ptr(ofi_server* s) { return ALIGNEDX(s->heap.get()); }
#define mv_server_init ofi_init
#define mv_server_serve ofi_serve
#define mv_server_send ofi_write_send
#define mv_server_rma ofi_write_rma
#define mv_server_rma_signal ofi_write_rma_signal
#define mv_server_heap_rkey ofi_heap_rkey
#define mv_server_heap_ptr ofi_heap_ptr
#define mv_server_finalize ofi_finalize

#endif
