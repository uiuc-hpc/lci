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

struct mpiv {
  int me;
  int size;
  vector<worker> w;
  PacketManager pkpool;
  mpiv_server server;
  mpiv_hash_tbl tbl;
  std::atomic<int> total_send;
} __attribute__((aligned(64)));

static mpiv MPIV;

double MPIV_Wtime() {
  using namespace std::chrono;
  return duration_cast<duration<double> >(
             high_resolution_clock::now().time_since_epoch())
      .count();
}

void* mpiv_malloc(size_t size) {
  void* ptr = MPIV.server.allocate((size_t)size);
  if (ptr == 0) throw std::runtime_error("no more memory\n");
  return ptr;
}

extern __thread int __wid;

inline int mpiv_worker_id() {
  return __wid;
}

#include "request.h"

void mpiv_free(void* ptr) {
    MPIV.server.deallocate(ptr);
}

#include "init.h"
#include "mpi/recv.h"
#include "mpi/irecv.h"
#include "mpi/send.h"
#include "mpi/waitall.h"
#include "coll/collective.h"
#endif
