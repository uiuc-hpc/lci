#ifndef SERVER_H_
#define SERVER_H_

#include <thread>
#include <iostream>
#include <memory>

#include "packet/packet_manager.h"
#include "profiler.h"
#include "affinity.h"

using std::unique_ptr;

void mpiv_serve_recv(mpiv_packet*);
void mpiv_serve_send(mpiv_packet*);
void mpiv_post_recv(mpiv_packet*);

double MPIV_Wtime();

struct pinned_pool {
  pinned_pool(void* ptr_) : ptr((uintptr_t)ptr_), last(0) {}

  uintptr_t ptr;
  std::atomic<size_t> last;

  void* allocate() {
    ptrdiff_t diff = (ptrdiff_t)(last.fetch_add(1) * sizeof(mpiv_packet));
    return (void*)(ptr + diff);
  }
};

class server_base {
 public:
  virtual inline void init(packet_manager& pkpool, int& rank, int& size) = 0;
  virtual inline void post_recv(mpiv_packet* p) = 0;
  virtual inline void serve() = 0;
  virtual inline void finalize() = 0;
  virtual inline void write_send(int rank, void* buf, size_t size, void* ctx) = 0;
  virtual inline void write_rma(int rank, void* from, void* to, uint32_t rkey, size_t size, void* ctx) = 0;
  virtual inline void* allocate(size_t s) = 0;
  virtual inline void deallocate(void* ptr) = 0;
  virtual inline uint32_t heap_rkey() = 0;
};

#include "server_rdmax.h"

using mpiv_server = server_rdmax;

#endif
