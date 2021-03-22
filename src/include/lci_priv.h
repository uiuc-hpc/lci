#ifndef LCI_PRIV_H_
#define LCI_PRIV_H_

#include "config.h"

#define SHORT_MSG_SIZE (LC_PACKET_SIZE - sizeof(struct packet_context))

#define LCI_SYNCL_PTR_TO_REQ_PTR(sync) (&((LCI_syncl_t*)sync)->request)

struct lc_server;
typedef struct lc_server lc_server;

extern lc_server** LCI_DEVICES;

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

typedef enum lc_ep_addr {
  EP_AR_DYN = 1 << 1,
  EP_AR_EXP = 1 << 2,
  EP_AR_IMM = 1 << 3,
} lc_ep_addr;

typedef enum lc_ep_ce {
  EP_CE_NULL = 0,
  EP_CE_SYNC = ((1 << 1) << 4),
  EP_CE_CQ = ((1 << 2) << 4),
  EP_CE_AM = ((1 << 3) << 4),
  EP_CE_GLOB = ((1 << 4) << 4),
} lc_ep_ce;

extern LCI_endpoint_t* LCI_ENDPOINTS;

struct LCI_PL_s {
  LCI_comm_t ctype;
  LCI_msg_t mtype;
  LCI_comp_t ltype;
  LCI_comp_t rtype;
  LCI_Handler handler;
  LCI_Allocator allocator;
  LCI_CQ_t cq;
  LCI_MT_t mt;
};

struct LCI_endpoint_s {
  // Associated hardware context.
  lc_server* server;
  long property;

  // Associated software components.
  lc_pool* pkpool;
  lc_rep* rep;
  lc_hash* mt;

  union {
    lc_cq* cq;
    LCI_Handler handler;
  };
  LCI_Allocator alloc;
  volatile int completed;

  int gid;
};

struct lc_rep {
  void* handle;
  int rank;
  intptr_t base;
  int32_t rkey;
};

#include "pool.h"
#include "packet.h"
#include "proto.h"
#include "server/server.h"

void lci_config_init(int num_proc, int rank);

static inline void lc_dev_init(int id, lc_server** dev)
{
  uintptr_t base_packet;
  lc_server_init(id, dev);
  lc_server* s = *dev;
  uintptr_t base_addr = (uintptr_t)lc_server_heap_ptr(s);
  base_packet = base_addr + 8192 - sizeof(struct packet_context);

  lc_pool_create(&s->pkpool);
  for (int i = 0; i < LC_SERVER_NUM_PKTS; i++) {
    lc_packet* p = (lc_packet*)(base_packet + i * LC_PACKET_SIZE);
    p->context.poolid = 0;
    // p->context.req_s.parent = p;
    lc_pool_put(s->pkpool, p);
  }

  s->curr_addr = base_packet + LC_SERVER_NUM_PKTS * LC_PACKET_SIZE;
  s->curr_addr = (s->curr_addr + 8192 - 1) / 8192 * 8192;
}

static inline void lc_dev_finalize(lc_server* dev) { lc_server_finalize(dev); }

#endif
