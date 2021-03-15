#ifndef LCI_PRIV_H_
#define LCI_PRIV_H_

#include "config.h"
#include "cq.h"

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

struct LCI_segment_s {
  uintptr_t mr_p;
  void *address;
  size_t length;
};

extern LCI_endpoint_t* LCI_ENDPOINTS;

struct LCI_plist_s {
  LCI_comm_t ctype;           // communication type
  LCI_match_t match_type;     // matching type
  LCI_msg_type_t mtype;            // message type
  LCI_comptype_t ltype;       // local completion type
  LCI_comptype_t rtype;       // remote completion type
  LCI_handler_t *handler;     // completion handler
  LCI_dynamic_t cdtype;       // dynamic type for command ports
  LCI_dynamic_t mdtype;       // dynamic type for message ports
  LCI_allocator_t allocator;  // dynamic allocator
  LCI_comp_t cq;              // completion queue
  LCI_MT_t mt;                // matching table
};

struct LCI_endpoint_s {
  // Associated hardware context.
  lc_server* server;
  uint64_t property;

  // Associated software components.
  lc_pool* pkpool;
  lc_rep* rep;
  lc_hash* mt;
  LCI_allocator_t alloc;

  union {
    lc_cq* cq;
    LCI_handler_t *handler;
  };
  volatile int completed;

  int gid;
};

/**
 * Internal context structure, Used by asynchronous operations to pass
 * information between initialization phase and completion phase.
 */
struct LCII_context_t {
  LCI_endpoint_t ep;
  LCI_data_t data;
  LCI_data_type_t data_type;
  LCI_msg_type_t msg_type;
  uint32_t rank;
  LCI_tag_t tag;
  LCI_comp_t comp;
  void* user_context;
};
typedef struct LCII_context_t LCII_context_t;

extern int lcg_deadlock;

#include "pool.h"
#include "packet.h"
#include "proto.h"
#include "lcii_register.h"
#endif
