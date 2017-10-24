#ifndef MPIV_LC_PRIV_H_
#define MPIV_LC_PRIV_H_

#include "lc.h"
#include "config.h"
#include <stdint.h>
#include <stdlib.h>
#include "lcrq.h"
#include "lc/hashtable.h"
#include "lc/pool.h"

#ifdef LC_USE_SERVER_OFI
#define SERVER_CONTEXT char __padding__[64 - 40];
typedef struct ofi_server lc_server;
#endif

#ifdef LC_USE_SERVER_IBV
#define SERVER_CONTEXT char __padding__[0];
typedef struct ibv_server lc_server;
#endif

#ifdef LC_USE_SERVER_PSM
#define SERVER_CONTEXT char __pandding__[64 - 40];
typedef struct psm_server lc_server;
#endif

struct lc_struct {
  int me;
  int size;
  int ncores;
  lc_server* server;
  lc_pool* pkpool;
  lc_hash* tbl;

#ifndef USE_CCQ
  struct dequeue* queue;
#else
  lcrq_t* queue;
#endif
} __attribute__((aligned(64)));

#include "packet.h"
#include "progress.h"
#include "server/server.h"
#include "proto.h"

#endif
