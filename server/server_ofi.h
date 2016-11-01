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

#define SPIN_LOCK(l)                                     \
  {                                                      \
    while (lock.test_and_set(std::memory_order_acquire)) \
      ;                                                  \
  }  // acquire lock
#define SPIN_UNLOCK(l) \
  { lock.clear(std::memory_order_release); }

class ServerOFI : ServerBase {
 public:
  ServerOFI() : stop_(false), done_init_(false) { lock.clear(); }
  inline void init(PacketManager& pkpool, int& rank, int& size);
  inline void post_recv(Packet* p);
  inline void serve();
  inline void finalize();
  inline void write_send(int rank, void* buf, size_t size, void* ctx);
  inline void write_rma(int rank, void* from, void* to, uint32_t rkey,
                        size_t size, void* ctx);
  inline void* allocate(size_t s);
  inline void deallocate(void* ptr);
  inline uint32_t heap_rkey() { return heap_rkey_; }

 private:
  inline bool progress();
  std::thread poll_thread_;
  volatile bool stop_;
  volatile bool done_init_;

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

  unique_ptr<char[]> heap_;
  unique_ptr<char[]> sbuf_;

  mbuffer heap_segment;
  uint32_t heap_rkey_;

  int recv_posted_;
  unique_ptr<pinned_pool> sbuf_alloc_;
  PacketManager* pk_mgr_ptr;
  std::atomic_flag lock;
};

void ServerOFI::init(PacketManager& pkpool, int& rank, int& size) {
#ifdef USE_AFFI
  affinity::set_me_to(0);
#endif

// Create hint.
#if 1
  struct fi_info* hints;
  hints = fi_allocinfo();
  hints->ep_attr->type = FI_EP_RDM;
  // hints->domain_attr->mr_mode = FI_MR_BASIC;
  hints->caps = FI_RMA | FI_MSG;
  hints->mode = FI_CONTEXT | FI_LOCAL_MR;
#endif

  // Create info.
  FI_SAFECALL(fi_getinfo(FI_VERSION(1, 0), NULL, NULL, 0, hints, &fi));

  // Create libfabric obj.
  FI_SAFECALL(fi_fabric(fi->fabric_attr, &fabric, NULL));

  // Create domain.
  FI_SAFECALL(fi_domain(fabric, fi, &domain, NULL));

  // Create end-point;
  FI_SAFECALL(fi_endpoint(domain, fi, &ep, NULL));

  // Create cq.
  struct fi_cq_attr cq_attr;
  memset(&cq_attr, 0, sizeof(cq_attr));
  cq_attr.format = FI_CQ_FORMAT_CONTEXT;
  cq_attr.size = MAX_CQ_SIZE;
  FI_SAFECALL(fi_cq_open(domain, &cq_attr, &scq, NULL));
  FI_SAFECALL(fi_cq_open(domain, &cq_attr, &rcq, NULL));

  // Bind my ep to cq.
  FI_SAFECALL(fi_ep_bind(ep, (fid_t)scq, FI_SEND | FI_TRANSMIT));
  FI_SAFECALL(fi_ep_bind(ep, (fid_t)rcq, FI_RECV));

  // Get memory for heap.
  heap_ = std::move(unique_ptr<char[]>(new char[HEAP_SIZE]));
  heap_segment = std::move(mbuffer(boost::interprocess::create_only,
                                   ALIGNEDX(heap_.get()), (size_t)HEAP_SIZE));

  FI_SAFECALL(fi_mr_reg(domain, heap_.get(), HEAP_SIZE,
                        FI_READ | FI_WRITE | FI_REMOTE_WRITE | FI_REMOTE_READ,
                        0, 0, 0, &mr_heap, 0));

  heap_rkey_ = fi_mr_key(mr_heap);

  sbuf_ = std::move(
      unique_ptr<char[]>(new char[sizeof(Packet) * (MAX_SEND + MAX_RECV + 2)]));
  FI_SAFECALL(fi_mr_reg(domain, sbuf_.get(),
                        sizeof(Packet) * (MAX_SEND + MAX_RECV + 2),
                        FI_READ | FI_WRITE | FI_REMOTE_WRITE | FI_REMOTE_READ,
                        0, 1, 0, &mr_sbuf, 0));
  sbuf_alloc_ = std::move(
      std::unique_ptr<pinned_pool>(new pinned_pool(ALIGNEDX(sbuf_.get()))));

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  // Now exchange end-point address and heap address.
  size_t addrlen = 0;
  fi_getname((fid_t)ep, NULL, &addrlen);
  void* addr = malloc(addrlen + sizeof(uintptr_t));
  FI_SAFECALL(fi_getname((fid_t)ep, addr, &addrlen));

  // Set heap address at the end. TODO(danghvu): Need a more generic way.
  uintptr_t myaddr = (uintptr_t)heap_.get();
  *(uintptr_t*)((char*)addr + addrlen) = myaddr;

  fi_addr.resize(size);
  heap_addr.resize(size);

  struct fi_av_attr av_attr;
  memset(&av_attr, 0, sizeof(av_attr));
  av_attr.type = FI_AV_MAP;
  FI_SAFECALL(fi_av_open(domain, &av_attr, &av, NULL));
  FI_SAFECALL(fi_ep_bind(ep, (fid_t)av, 0));
  FI_SAFECALL(fi_enable(ep));

  for (int i = 0; i < size; i++) {
    if (i != rank) {
      void* destaddr = malloc(addrlen + sizeof(uintptr_t));
      MPI_Sendrecv(addr, addrlen + sizeof(uintptr_t), MPI_BYTE, i, 99, destaddr,
                   addrlen + sizeof(uintptr_t), MPI_BYTE, i, 99, MPI_COMM_WORLD,
                   MPI_STATUS_IGNORE);
      if (fi_av_insert(av, destaddr, 1, &fi_addr[i], 0, NULL) == -1) {
        MPI_Abort(MPI_COMM_WORLD, -1);
      }
      heap_addr[i] = *(uintptr_t*)((char*)destaddr + addrlen);
    }
  }

  // Prepare the packet_mgr and prepost some packet.
  for (int i = 0; i < MAX_SEND; i++) {
    pkpool.ret_packet((Packet*)sbuf_alloc_->allocate());
  }

  for (int i = 0; i < MAX_RECV; i++) {
    pkpool.ret_packet((Packet*)sbuf_alloc_->allocate());
  }
  pk_mgr_ptr = &pkpool;
  done_init_ = true;
}

struct my_context {
  my_context(void* ctx) : ctx_(ctx) {}
  struct fi_context fi_ctx;
  void* ctx_;
};

bool ServerOFI::progress() {  // profiler& p, long long& r, long long &s) {
  initt(t);
  startt(t);

  struct fi_cq_entry entry;
  struct fi_cq_err_entry error;
  ssize_t ret;
  bool rett = false;

  ret = fi_cq_read(rcq, &entry, 1);
  if (ret > 0) {
    // Got an entry here ?
    recv_posted_--;
    mpiv_serve_recv((Packet*)(((my_context*)entry.op_context)->ctx_));
    delete (my_context*)entry.op_context;
    rett = true;
  } else if (ret == -FI_EAGAIN) {
  } else if (ret == -FI_EAVAIL) {
    fi_cq_readerr(rcq, &error, 0);
    printf("Err: %s\n", fi_strerror(error.err));
    MPI_Abort(MPI_COMM_WORLD, error.err);
  } else if (ret < 0) {
    /* handle error */
    printf("Err: %s\n", fi_strerror(ret));
    MPI_Abort(MPI_COMM_WORLD, error.err);
  }

  ret = fi_cq_read(scq, &entry, 1);
  if (ret > 0) {
    // Got an entry here ?
    mpiv_serve_send((Packet*)(((my_context*)entry.op_context)->ctx_));
    delete (my_context*)entry.op_context;
    rett = true;
  } else if (ret == -FI_EAGAIN) {
  } else if (ret == -FI_EAVAIL) {
    fi_cq_readerr(scq, &error, 0);
    printf("Err: %s\n", fi_strerror(error.err));
    MPI_Abort(MPI_COMM_WORLD, error.err);
  } else if (ret < 0) {
    /* handle error */
    printf("Err: %s\n", fi_strerror(ret));
    MPI_Abort(MPI_COMM_WORLD, error.err);
  }

  if (recv_posted_ < MAX_RECV) post_recv(pk_mgr_ptr->get_for_recv());

  stopt(t) return rett;
}

void ServerOFI::serve() {
  poll_thread_ = std::thread([this] {
    __wid = -1;
#ifdef USE_AFFI
    affinity::set_me_to_last();
#endif

#ifdef USE_PAPI
    profiler server({PAPI_L1_DCM});
    server.start();
#endif

    while (xunlikely(!this->stop_)) {
      while (progress()) {
      };
    }

#ifdef USE_PAPI
    server.stop();
    server.print();
#endif
  });
}

void ServerOFI::post_recv(Packet* p) {
  if (p == NULL) return;
  SPIN_LOCK(lock);
  FI_SAFECALL(
      fi_recv(ep, p, sizeof(Packet), 0, FI_ADDR_UNSPEC, new my_context(p)));
  recv_posted_++;
  SPIN_UNLOCK(lock);
}

void ServerOFI::write_send(int rank, void* buf, size_t size, void* ctx) {
  SPIN_LOCK(lock);
  FI_SAFECALL(fi_send(ep, buf, size, 0, fi_addr[rank], new my_context(ctx)));
  SPIN_UNLOCK(lock);
}

void ServerOFI::write_rma(int rank, void* from, void* to, uint32_t rkey,
                          size_t size, void* ctx) {
  SPIN_LOCK(lock);
  FI_SAFECALL(fi_write(ep, from, size, 0, fi_addr[rank],
                       (uintptr_t)to - heap_addr[rank],  // this is offset.
                       rkey, new my_context(ctx)));
  SPIN_UNLOCK(lock);
}

void* ServerOFI::allocate(size_t s) { return heap_segment.allocate(s); }

void ServerOFI::deallocate(void* ptr) { heap_segment.deallocate(ptr); }

void ServerOFI::finalize() {
  stop_ = true;
  poll_thread_.join();
}

#endif
