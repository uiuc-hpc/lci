#ifndef MPIV_MV_PRIV_H_
#define MPIV_MV_PRIV_H_

#include "lc.h"
#include "config.h"
#include <stdint.h>
#include <stdlib.h>
#include "lcrq.h"
#include "dequeue.h"

#ifdef MV_USE_SERVER_OFI
#define SERVER_CONTEXT char __padding__[64 - 40];
typedef struct ofi_server lc_server;
#endif

#ifdef MV_USE_SERVER_IBV
#define SERVER_CONTEXT char __padding__[0];
typedef struct ibv_server lc_server;
#endif

#ifdef MV_USE_SERVER_PSM
#define SERVER_CONTEXT char __pandding__[64 - 40];
typedef struct psm_server lc_server;
#endif

struct lc_struct {
  int me;
  int size;
  lc_server* server;
  lc_pool* pkpool;
  lc_pool* rma_pool;
  lc_hash* tbl;
#ifndef USE_CCQ
  struct dequeue queue;
#else
  lcrq_t queue;
#endif
  // int am_table_size;
  // lc_am_func_t am_table[128];
} __attribute__((aligned(64)));

#define RMA_SIGNAL_SIMPLE (1 << 30)
#define RMA_SIGNAL_QUEUE (1 << 31)

#include "packet.h"
#include "pool.h"
#include "hashtable.h"
#include "progress.h"
#include "server/server.h"
#include "proto.h"

#endif
