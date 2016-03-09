#ifndef MPIV_H_
#define MPIV_H_

#include <mpi.h>
#include <boost/lockfree/stack.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/interprocess/managed_external_buffer.hpp>
#include <boost/interprocess/creation_tags.hpp>

#include <vector>
#include <atomic>

#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>

#include "fult.h"

#include "except.h"
#include "rdmax.h"
#include "request.h"
#include "hashtbl.h"
#include "packet_manager.h"

#include "common.h"

struct mpiv_ctx {
  uint32_t sbuf_lkey;
  uint32_t heap_rkey;
  uint32_t heap_lkey;
  mbuffer heap_segment;
  vector<connection> conn;
};

#include "server.h"

struct alignas(64) mpiv {
  int me;
  int size;
  vector<worker> w;
  mpiv_ctx ctx;
  packet_manager2 sendpk;
  packet_manager recvpk;
  mpiv_server server;

  mpiv_hash_tbl tbl;
};

static mpiv MPIV;

double MPIV_Wtime() {
  using namespace std::chrono;
  return duration_cast<duration<double> >(
             high_resolution_clock::now().time_since_epoch())
      .count();
}


void* mpiv_malloc(size_t size) {
  void* ptr = MPIV.ctx.heap_segment.allocate((size_t)size);
  if (ptr == 0)
    throw std::runtime_error("no more memory\n");
  return ptr;
}

void mpiv_free(void* ptr) { MPIV.ctx.heap_segment.deallocate(ptr); }

void mpiv_send_recv_ready(MPIV_Request* sreq, MPIV_Request* rreq) {
  // Need to write them back, setup as a RECV_READY.
  char data[RNDZ_MSG_SIZE];
  mpiv_packet* p = (mpiv_packet*) data;
  p->set_header(RECV_READY, MPIV.me, rreq->tag);
  p->set_rdz((uintptr_t)sreq, (uintptr_t)rreq, (uintptr_t)rreq->buffer,
            MPIV.ctx.heap_rkey);
  MPIV.ctx.conn[rreq->rank].write_send((void*)p, RNDZ_MSG_SIZE, 0, 0);
}

#include "init.h"
#include "recv.h"
#include "send.h"

#endif
