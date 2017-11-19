#ifndef MPIV_LC_PRIV_H_
#define MPIV_LC_PRIV_H_

#include "config.h"
#include <stdint.h>
#include <stdlib.h>
#include "lc/hashtable.h"
#include "lc/pool.h"

#if 0
#ifdef LC_USE_SERVER_OFI
#define SERVER_CONTEXT char __padding__[64 - 40];
#endif

#ifdef LC_USE_SERVER_IBV
#define SERVER_CONTEXT char __padding__[0];
#endif

#ifdef LC_USE_SERVER_PSM
#define SERVER_CONTEXT char __pandding__[64 - 40];
#endif
#endif

struct lc_struct {
  int me;
  int size;
  int ncores;
  void* heap_base;
  void* server;
  lc_pool* pkpool;
  lc_hash* tbl;

#ifndef USE_CCQ
  struct dequeue* queue;
#else
  lcrq_t* queue;
#endif
} __attribute__((aligned(64)));

#endif
