#ifndef SERVER_H_
#define SERVER_H_

#include "config.h"
#include <iostream>
#include <memory>
#include <thread>

#include "affinity.h"
#include "packet/packet_manager.h"
#include "profiler.h"

using std::unique_ptr;

namespace mpiv {

void mpiv_recv_imm(uint32_t imm);
void mpiv_serve_recv(Packet*);
void mpiv_serve_send(Packet*);
void mpiv_post_recv(Packet*);

double MPIV_Wtime();

struct pinned_pool {
  pinned_pool(void* ptr_) : ptr((uintptr_t)ptr_), last(0) {}

  uintptr_t ptr;
  std::atomic<size_t> last;

  void* allocate() {
    ptrdiff_t diff = (ptrdiff_t)(last.fetch_add(1) * sizeof(Packet));
    return (void*)(ptr + diff);
  }
};

class ServerBase {
 public:
  virtual void init(PacketManager& pkpool, int& rank, int& size) = 0;
  virtual void post_recv(Packet* p) = 0;
  virtual void serve() = 0;
  virtual void finalize() = 0;
  virtual void write_send(int rank, void* buf, size_t size,
      void* ctx) = 0;
  virtual void write_rma(int rank, void* from, uint32_t lkey, void* to, uint32_t rkey,
      size_t size, void* ctx) = 0;
  virtual void* allocate(size_t s) = 0;
  virtual void deallocate(void* ptr) = 0;
  virtual uint32_t heap_rkey() = 0;
};

#include "server_ofi.h"
#include "server_rdmax.h"

}  // namespace mpiv.
#endif
