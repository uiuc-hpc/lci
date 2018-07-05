#ifndef LC_PRIV_H_
#define LC_PRIV_H_

#include "lc/macro.h"
#include "lc/pool.h"
#include "lc/hashtable.h"

#include "cq.h"

extern struct lci_hw* hw;
extern int lcg_nep;
extern int lcg_size;
extern int lcg_rank;
extern char lcg_name[256];
extern lc_ep lcg_ep_list[256];

typedef struct lci_hw lci_hw;

typedef void* (*lc_alloc_fn)(size_t);
typedef void (*lc_free_fn)(void*);

// remote endpoint is just a handle.
struct lci_rep {
  lc_eid rank;
  lc_eid eid;
  void* handle;
};

typedef struct lci_ep {
  lc_eid eid;

  // Associated hardware context.
  lci_hw* hw;

  // Cap
  long cap;

  // Other misc data.
  lc_alloc_fn alloc;
  lc_free_fn free;
  lc_hash* tbl;
  struct comp_q cq;
} lci_ep;

struct lci_hw {
  // hw-indepdentent.
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

LC_INLINE
void lci_hw_init(struct lci_hw* hw)
{
  // This initialize the hardware context.
  // There might be more than one remote connection.
  lc_server_init(hw);

  uintptr_t base_packet = 0;
  posix_memalign((void**) &base_packet, 4096, SERVER_NUM_PKTS * LC_PACKET_SIZE * 2 + 4096);
  hw->base_addr = base_packet;
  lc_pool_create(&hw->pkpool);
  for (unsigned i = 0; i < SERVER_NUM_PKTS; i++) {
      lc_packet* p = (lc_packet*) (base_packet + i * LC_PACKET_SIZE);
      p->context.poolid  = 0;
      p->context.runtime = 0;
      p->context.req_s.parent = p;
      lc_pool_put(hw->pkpool, p);
  }
}

LC_INLINE
void lci_ep_open(struct lci_hw* hw, struct lci_ep** ep_ptr, long cap)
{
  struct lci_ep* ep;
  posix_memalign((void**) &ep, 4096, sizeof(struct lci_ep));

  ep->hw = hw;
  ep->cap = cap;
  ep->eid = lcg_nep++;
  lcg_ep_list[ep->eid] = ep;

  lc_pm_publish(lcg_rank, ep->eid, lcg_name, hw->name);

  lc_hash_create(&ep->tbl);
  cq_init(&ep->cq);
  ep->alloc = fixed_buffer_allocator;
  ep->free = fixed_buffer_allocator_free;

  *ep_ptr = ep;
}

LC_INLINE
void lci_ep_connect(int hwid, int prank, int erank, lc_rep* rep)
{
  lc_server_connect(&hw[hwid], prank, erank, rep);
}


#endif
