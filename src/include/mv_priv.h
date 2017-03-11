#ifndef MPIV_MV_PRIV_H_
#define MPIV_MV_PRIV_H_

#include "mv.h"
#include "config.h"
#include <stdint.h>
#include <stdlib.h>
#include "lcrq.h"
#include "dequeue.h"

#ifdef MV_USE_SERVER_OFI
#define SERVER_CONTEXT char __padding__[64 - 40];
typedef struct ofi_server mv_server;
#endif

#ifdef MV_USE_SERVER_IBV
#define SERVER_CONTEXT char __padding__[0];
typedef struct ibv_server mv_server;
#endif

#ifdef MV_USE_SERVER_PSM
#define SERVER_CONTEXT char __pandding__[64 - 40];
typedef struct psm_server mv_server;
#endif

struct mv_struct {
  int me;
  int size;
  mv_server* server;
  mv_pool* pkpool;
  mv_pool* rma_pool;
  mv_hash* tbl;
#ifndef USE_CCQ
  struct dequeue queue;
#else
  lcrq_t queue;
#endif
  // int am_table_size;
  // mv_am_func_t am_table[128];
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
