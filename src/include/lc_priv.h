#ifndef LC_PRIV_H_
#define LC_PRIV_H_

#include "config.h"
#include "lc/macro.h"
#include "lc/pool.h"
#include "lc/hashtable.h"

#include "cq.h"

#define SHORT_MSG_SIZE (LC_PACKET_SIZE - sizeof(struct packet_context))
#define POST_MSG_SIZE (SHORT_MSG_SIZE)

extern struct lci_dev* dev;
extern struct lci_ep** lcg_ep_list;

extern int lcg_nep;
extern int lcg_size;
extern int lcg_rank;
extern int lcg_page_size;
extern char lcg_name[256];

typedef struct lci_dev lci_dev;

// remote endpoint is just a handle.
struct lci_rep {
  void* handle;
  int rank;
  int gid;
  uintptr_t base;
  uint32_t rkey;
} __attribute__((packed, aligned(LC_CACHE_LINE)));

struct lci_ep {
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
    lc_handler_fn handler;
  };

  struct lci_rep* rep;

  struct comp_q cq __attribute__((aligned(LC_CACHE_LINE)));
} __attribute__((packed, aligned(LC_CACHE_LINE)));

struct lci_dev {
  // dev-indepdentent.
  void* handle;
  char* name;

  // LC_CACHE_LINE-bit cap.
  long cap;

  lc_pool* pkpool;
  uintptr_t base_addr;
  uintptr_t curr_addr;
} __attribute__((packed, aligned(LC_CACHE_LINE)));

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
  base_packet = dev->base_addr + lcg_page_size - sizeof(struct packet_context);

  lc_pool_create(&dev->pkpool);
  for (unsigned i = 0; i < LC_SERVER_NUM_PKTS; i++) {
      lc_packet* p = (lc_packet*) (base_packet + i * LC_PACKET_SIZE);
      p->context.poolid  = 0;
      p->context.req_s.parent = p;
      lc_pool_put(dev->pkpool, p);
  }

  dev->curr_addr = base_packet + LC_SERVER_NUM_PKTS * LC_PACKET_SIZE;
  dev->curr_addr = (dev->curr_addr + lcg_page_size - 1) / lcg_page_size * lcg_page_size;
}

LC_INLINE
void lci_ep_open(struct lci_dev* dev, long cap, struct lci_ep** ep_ptr)
{
  struct lci_ep* ep = 0;
  posix_memalign((void**) &ep, LC_CACHE_LINE, sizeof(struct lci_ep));
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
  posix_memalign((void**) &(ep->rep), LC_CACHE_LINE, sizeof(struct lci_rep) * lcg_size);

  for (int i = 0; i < lcg_size; i++)
    if (i != lcg_rank)
      lc_server_connect(dev->handle, i, ep->gid, &ep->rep[i]);

  *ep_ptr = ep;
}

#endif
