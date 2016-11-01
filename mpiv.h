#ifndef MPIV_H_
#define MPIV_H_

#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/managed_external_buffer.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/stack.hpp>
#include <mpi.h>

#include <atomic>
#include <deque>
#include <vector>

#include <malloc.h>
#include <stdlib.h>
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

void free(void* ptr) { MPIV.server.deallocate(ptr); }

};  // namespace mpiv.

#include "init.h"
#include "mpi/mpi.h"
#include "request.h"

#endif
