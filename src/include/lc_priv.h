#ifndef LC_PRIV_H_
#define LC_PRIV_H_

#include "lc/macro.h"
#include "lc/pool.h"
#include "lc/hashtable.h"

#include "cq.h"

extern struct lci_dev* dev;
extern int lcg_nep;
extern int lcg_size;
extern int lcg_rank;
extern char lcg_name[256];
extern lc_ep lcg_ep_list[256];

typedef struct lci_dev lci_dev;

// remote endpoint is just a handle.
struct lci_rep {
  void* handle;
  int rank;
  int gid;
} __attribute__((aligned(64)));

typedef struct lci_ep {
  int gid;

  // Associated hardware context.
  lci_dev* dev;
  lc_pool* pkpool;
  void* handle;

  // Cap
  long cap;

  // Other misc data.
  union{
  struct {
    lc_alloc_fn alloc;
    lc_free_fn free;
    void* ctx;
  };

  lc_hash* tbl;
  };

  struct comp_q cq;

  struct lci_rep* rep;
} lci_ep;

struct lci_dev {
  // dev-indepdentent.
  void* handle;
  char* name;

  // 64-bit cap.
  long cap;

  lc_pool* pkpool;
  uintptr_t base_addr;
};

struct lc_packet;
typedef struct lc_packet lc_packet;

#include "packet.h"
#include "proto.h"
#include "server/server.h"

LC_INLINE
void lci_dev_init(struct lci_dev* dev)
{
  uintptr_t base_packet;

  // This initialize the hardware context.
  // There might be more than one remote connection.
  lc_server_init(dev);

  dev->base_addr = (uintptr_t) lc_server_heap_ptr(dev->handle);
  base_packet = dev->base_addr;

  lc_pool_create(&dev->pkpool);
  for (unsigned i = 0; i < SERVER_NUM_PKTS; i++) {
      lc_packet* p = (lc_packet*) (base_packet + i * LC_PACKET_SIZE);
      p->context.poolid  = 0;
      p->context.req_s.parent = p;
      lc_pool_put(dev->pkpool, p);
  }
}

LC_INLINE
void lci_ep_open(struct lci_dev* dev, long cap, struct lci_ep** ep_ptr)
{
  struct lci_ep* ep;
  posix_memalign((void**) &ep, 64, sizeof(struct lci_ep));

  ep->dev = dev;
  ep->handle = ep->dev->handle;
  ep->pkpool = dev->pkpool;
  ep->cap = cap;
  ep->gid = lcg_nep++;
  lcg_ep_list[ep->gid] = ep;

  if (cap & EP_AR_EXPL)
    lc_hash_create(&ep->tbl);
  else {
    cq_init(&ep->cq);
  }

  lc_server_ep_publish(dev->handle, ep->gid);
  posix_memalign((void**) &(ep->rep), 64, sizeof(struct lci_rep) * lcg_size);

  for (int i = 0; i < lcg_size; i++)
    if (i != lcg_rank)
      lc_server_connect(dev->handle, i, ep->gid, &ep->rep[i]);

  *ep_ptr = ep;
}

#endif
