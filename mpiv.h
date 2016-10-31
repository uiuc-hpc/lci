#ifndef MPIV_H_
#define MPIV_H_

#include <mpi.h>
#include <boost/lockfree/stack.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/interprocess/managed_external_buffer.hpp>
#include <boost/interprocess/creation_tags.hpp>

#include <vector>
#include <deque>
#include <atomic>

#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>

#include "ult.h"

#include "except.h"
#include "rdmax.h"

#include "hashtable/hashtbl.h"
#include "packet/packet_manager.h"

#include "common.h"

#include "server/server.h"

extern __thread int __wid;
inline int worker_id() { return __wid; }

namespace mpiv {

struct Execution {
  int me;
  int size;
  PacketManager pkpool;
  Server server;
  HashTbl tbl;
  std::vector<worker> w;
} __attribute__((aligned(64)));

static Execution MPIV;

void* malloc(size_t size) {
  void* ptr = MPIV.server.allocate((size_t)size);
  if (ptr == 0) throw std::runtime_error("no more memory\n");
  return ptr;
}

double wtime() {
  using namespace std::chrono;
  return duration_cast<duration<double> >(
             high_resolution_clock::now().time_since_epoch())
      .count();
}

void free(void* ptr) {
    MPIV.server.deallocate(ptr);
}

}; // namespace mpiv.

#include "request.h"
#include "init.h"
#include "mpi/mpi.h"

#endif
