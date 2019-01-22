#ifndef LCI_PRIV_H_
#define LCI_PRIV_H_

#include "config.h"

#define SHORT_MSG_SIZE (LC_PACKET_SIZE - sizeof(struct packet_context))

struct lc_server;
typedef struct lc_server lc_server;

struct lc_packet;
typedef struct lc_packet lc_packet;

struct lc_pool;
typedef struct lc_pool lc_pool;

struct lc_hash;
typedef struct lc_hash lc_hash;

struct lc_cq;
typedef struct lc_cq lc_cq;

struct lc_req;
typedef struct lc_rep lc_rep;

typedef void (*lc_handler)(LCI_Request* req);
typedef void* (*lc_alloc)(size_t size, void* usr_ctx);

typedef enum lc_ep_addr {
  EP_AR_DYN = 1<<1,
  EP_AR_EXP  = 1<<2,
  EP_AR_IMM  = 1<<3,
} lc_ep_addr;

typedef enum lc_ep_ce {
  EP_CE_NULL = 0,
  EP_CE_SYNC = ((1<<1) << 4),
  EP_CE_CQ   = ((1<<2) << 4),
  EP_CE_AM   = ((1<<3) << 4),
  EP_CE_GLOB = ((1<<4) << 4),
} lc_ep_ce;

extern LCI_Endpoint lcg_endpoint[];
extern int lcg_num_devices;
extern int lcg_num_endpoints;
extern int lcg_rank;
extern int lcg_size;

struct LCI_Property_s {
  LCI_Comm_type ctype;
  LCI_Message_type mtype;
  LCI_Sync_type stype;
  void* s_ctx;
};

struct LCI_Endpoint_s {
  // Associated hardware context.
  lc_server* server;
  long property;

  // Associated software components.
  lc_pool* pkpool;
  lc_rep* rep;
  union {
    lc_hash* tbl;
    lc_cq* cq;
    lc_handler handler;
  };
  lc_alloc alloc;
  volatile int completed;

  int gid;
};

struct LCI_Sync_s {
  volatile int flag;
};

struct lc_rep {
  void* handle;
  int rank;
  intptr_t base;
  int32_t rkey;
};

#include "lc/pool.h"
#include "packet.h"
#include "proto.h"
#include "server/server.h"

static inline void lc_dev_init(int id, lc_server** dev)
{
  uintptr_t base_packet;
  lc_server_init(id, dev);
  lc_server* s = *dev;
  uintptr_t base_addr = (uintptr_t) lc_server_heap_ptr(s);
  base_packet = base_addr + 8192 - sizeof(struct packet_context);

  lc_pool_create(&s->pkpool);
  for (int i = 0; i < LC_SERVER_NUM_PKTS; i++) {
    lc_packet* p = (lc_packet*) (base_packet + i * LC_PACKET_SIZE);
    p->context.poolid  = 0;
    // p->context.req_s.parent = p;
    lc_pool_put(s->pkpool, p);
  }
}

static inline void lc_dev_finalize(lc_server* dev)
{
}

#endif
