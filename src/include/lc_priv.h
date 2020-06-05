#ifndef LC_PRIV_H_
#define LC_PRIV_H_

#include "config.h"
#include "lc/macro.h"
#include "lc/pool.h"
#include "lc/hashtable.h"

#include "cq.h"

#define SHORT_MSG_SIZE (LC_PACKET_SIZE - sizeof(struct packet_context))
#define POST_MSG_SIZE (SHORT_MSG_SIZE)

extern int lcg_nep;
extern int lcg_size;
extern int lcg_rank;
extern int lcg_page_size;
extern char lcg_name[256];

struct lc_server;
typedef struct lc_server lc_server;

// remote endpoint is just a handle.
struct lci_rep {
  void* handle;
  int rank;
  uintptr_t base;
  uint32_t rkey;
} __attribute__((packed, aligned(LC_CACHE_LINE)));

struct lci_ep {
  // Cap
  long cap;
  int gid;
  volatile int completed;

  // Associated hardware context.
  lc_server* server;
  lc_pool* pkpool;
  struct lci_rep* rep;

  // Other misc data.
  union{
    lc_alloc_fn alloc;
    lc_hash* tbl;
  };

  union {
    lc_handler_fn handler;
    struct comp_q cq __attribute__((aligned(LC_CACHE_LINE)));
  };
} __attribute__((packed, aligned(LC_CACHE_LINE)));


struct lc_packet;
typedef struct lc_packet lc_packet;

#include "packet.h"
#include "proto.h"
#include "server/server.h"

LC_INLINE
void lci_dev_init(int id, lc_server** dev)
{
  uintptr_t base_packet;
  lc_server_init(id, dev);
  lc_server* s = *dev;

  uintptr_t base_addr = (uintptr_t) lc_server_heap_ptr(s);
  base_packet = base_addr + lcg_page_size - sizeof(struct packet_context);

  lc_pool_create(&s->pkpool);
  for (unsigned i = 0; i < LC_SERVER_NUM_PKTS; i++) {
      lc_packet* p = (lc_packet*) (base_packet + i * LC_PACKET_SIZE);
      p->context.poolid  = 0;
      p->context.req_s.parent = p;
      lc_pool_put(s->pkpool, p);
  }

  s->curr_addr = base_packet + LC_SERVER_NUM_PKTS * LC_PACKET_SIZE;
  s->curr_addr = (s->curr_addr + lcg_page_size - 1) / lcg_page_size * lcg_page_size;
}

LC_INLINE
void lci_ep_open(lc_server* dev, long cap, struct lci_ep** ep_ptr)
{
  struct lci_ep* ep = 0;
  posix_memalign((void**) &ep, LC_CACHE_LINE, sizeof(struct lci_ep));
  ep->server = dev;
  ep->pkpool = dev->pkpool;
  ep->cap = cap;
  ep->gid = lcg_nep++;

  lcg_ep[ep->gid] = ep;

  if (cap & EP_AR_EXP)
    lc_hash_create(&ep->tbl);
  else {
    cq_init(&ep->cq);
  }

  ep->rep = dev->rep;
  *ep_ptr = ep;
}

#endif
