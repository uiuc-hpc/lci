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

#include "fult.h"

#include "except.h"
#include "rdmax.h"

#include "hashtable/hashtbl.h"
#include "packet/packet_manager.h"

#include "common.h"

struct mpiv_ctx {
  uint32_t sbuf_lkey;
  uint32_t heap_rkey;
  uint32_t heap_lkey;
  mbuffer heap_segment;
  vector<connection> conn;
};

#include "server.h"

struct mpiv {
  int me;
  int size;
  vector<worker> w;
  mpiv_ctx ctx;
  packet_manager pkpool;
  mpiv_server server;
  mpiv_hash_tbl tbl;
  std::atomic<uint8_t> total_send;
} __attribute__((aligned(64)));

static mpiv MPIV;

double MPIV_Wtime() {
  using namespace std::chrono;
  return duration_cast<duration<double> >(
             high_resolution_clock::now().time_since_epoch())
      .count();
}

void* mpiv_malloc(size_t size) {
  void* ptr = MPIV.ctx.heap_segment.allocate((size_t)size);
  if (ptr == 0) throw std::runtime_error("no more memory\n");
  return ptr;
}

#include "request.h"

void mpiv_free(void* ptr) { MPIV.ctx.heap_segment.deallocate(ptr); }

#include "init.h"
#include "recv.h"
#include "send.h"
#include "coll/collective.h"
#endif
