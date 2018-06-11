#ifndef LC_PRIV_H_
#define LC_PRIV_H_

#include "lc/macro.h"
#include "lc/pool.h"
#include "lc/hashtable.h"

#include "cq.h"

typedef struct lci_hw lci_hw;

typedef struct lci_conn {
  int n_handle;
  void** handle;
} lci_conn;

typedef void* (*lc_alloc_fn)(size_t);
typedef void (*lc_free_fn)(void*);

typedef struct lci_ep {
  lc_eid eid;

  // Associated hardware context.
  void* hw;

  // Remote hardware connections.
  lci_conn remotes; 
  
  // Other misc data.
  lc_alloc_fn alloc;
  lc_free_fn free;
  uintptr_t base_addr;
  lc_pool* pkpool;
  lc_hash* tbl;
  struct comp_q cq;
} lci_ep;

struct lci_hw {
  // hw-indepdentent.
  void* handle;

  // 64-bit cap.
  long cap;

  // master end-point.
  lci_ep master_ep;
};

struct lc_packet;
typedef struct lc_packet lc_packet;

#include "packet.h"
#include "proto.h"
#include "server/server.h"

static void* fixed_buffer_allocator(size_t size)
{
  static void* p = 0;
  if (unlikely(!p)) {
    long sz = sysconf(_SC_PAGESIZE);
    p = malloc(4 * 1024 *1024);
    p = (void*) (((uintptr_t) p + sz - 1) / sz * sz);
  }
  return p;
}

static void fixed_buffer_allocator_free(void* ptr __UNUSED__)
{
  // nothing to do.
}

static inline
void lci_hw_init(struct lci_hw* hw, long cap)
{
  // This initialize the hardware context.
  // There might be more than one remote connection.
  lc_server_init(hw, &hw->master_ep.eid);
  hw->cap = cap;
  lci_ep* ep = &hw->master_ep;

  uintptr_t base_packet = 0;
  posix_memalign((void**) &base_packet, 4096, SERVER_NUM_PKTS * LC_PACKET_SIZE * 2 + 4096);
  ep->base_addr = base_packet;

  lc_hash_create(&ep->tbl);
  lc_pool_create(&ep->pkpool);
  for (unsigned i = 0; i < SERVER_NUM_PKTS; i++) {
    lc_packet* p = (lc_packet*) (base_packet + i * LC_PACKET_SIZE);
    p->context.poolid  = 0;
    p->context.runtime = 0;
    p->context.req_s.parent = p;
    lc_pool_put(ep->pkpool, p);
  }
  cq_init(&ep->cq);
  ep->alloc = fixed_buffer_allocator;
  ep->free = fixed_buffer_allocator_free;
}

#endif
