#ifndef MPIV_H_
#define MPIV_H_

#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/managed_external_buffer.hpp>
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

#include "hashtable.h"
#include "packet.h"
#include "packet_pool.h"

#include "common.h"

#include "server/server.h"

typedef void (*mv_am_func_t)();

struct Execution {
  int me;
  int size;
  Server server;
  mv_hash* tbl;
  mv_pp* pkpool;
  std::vector<mv_worker> w;
  std::vector<mv_am_func_t> am_table;
} __attribute__((aligned(64)));

static Execution MPIV;

void* mv_malloc(size_t size) {
  void* ptr = MPIV.server.allocate((size_t)size);
  if (ptr == 0) throw std::runtime_error("no more memory\n");
  return ptr;
}

void mv_free(void* ptr) { MPIV.server.deallocate(ptr); }

#include "request.h"
#include "init.h"
#include "mv/send.h"
#include "mv/recv.h"
#include "mv/isend.h"
#include "mv/irecv.h"
#include "mv/waitall.h"

#endif
