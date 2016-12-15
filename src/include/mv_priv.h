#ifndef MPIV_MV_PRIV_H_
#define MPIV_MV_PRIV_H_

#include "mv.h"
#include <stdint.h>
#include <stdlib.h>

#ifdef MV_USE_SERVER_OFI
#define SERVER_CONTEXT char __padding__[64];
typedef struct ofi_server mv_server;
#endif

#ifdef MV_USE_SERVER_IBV
#define SERVER_CONTEXT char __padding__[0];
typedef struct ibv_server mv_server;
#endif

struct mv_struct {
  int me;
  int size;
  mv_server* server;
  mv_pool* pkpool;
  mv_hash* tbl;
  int am_table_size;
  mv_am_func_t am_table[128];
} __attribute__((aligned(64)));

#include "packet.h"
#include "request.h"
#include "pool.h"
#include "hashtable.h"
#include "progress.h"
#include "server/server.h"
#include "proto.h"

#endif
