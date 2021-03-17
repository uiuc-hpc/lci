#ifndef LCI_PRIV_H_
#define LCI_PRIV_H_

#include "config.h"
#include "cq.h"
#include "lcii_register.h"

#define LCI_SYNCL_PTR_TO_REQ_PTR(sync) (&((LCI_syncl_t*)sync)->request)

struct lc_server;
typedef struct lc_server lc_server;

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


struct LCI_plist_s {
  LCI_match_t match_type;     // matching type
  LCI_comptype_t cmd_comp_type; // source-side completion type
  LCI_comptype_t msg_comp_type; // destination-side completion type
  LCI_allocator_t allocator;  // dynamic allocator
};

struct LCI_endpoint_s {
  // Associated hardware context.
  lc_server* server;
  uint64_t property;

  // Associated software components.
  lc_pool* pkpool;
  lc_rep* rep;
  LCI_MT_t mt;
  LCII_register_t ctx_reg; // used for long message protocol

  // user-defined components
  LCI_match_t match_type;     // matching type (tag/ranktag)
  LCI_comptype_t cmd_comp_type; // command-port completion type
  LCI_comptype_t msg_comp_type; // message-port completion type
  LCI_allocator_t allocator;  // dynamic allocator @note redundant for now

  volatile int completed;

  int gid;
};

/**
 * Internal context structure, Used by asynchronous operations to pass
 * information between initialization phase and completion phase.
 */
struct LCII_context_t {
  int id; // used by the long message protocol
  LCI_endpoint_t ep;
  LCI_data_t data;
  LCI_data_type_t data_type;
  LCI_msg_type_t msg_type;
  uint32_t rank;
  LCI_tag_t tag;
  LCI_comp_t completion;
  void* user_context;
};
typedef struct LCII_context_t LCII_context_t;

extern lc_server** LCI_DEVICES;
extern LCI_plist_t* LCI_PLISTS;
extern LCI_endpoint_t* LCI_ENDPOINTS;
extern int lcg_deadlock;

static inline LCI_request_t LCII_ctx2req(LCII_context_t *ctx) {
  LCI_request_t request = {
      .flag = LCI_OK,
      .rank = ctx->rank,
      .tag = ctx->tag,
      .type = ctx->data_type,
      .data = ctx->data,
      .user_context = ctx->user_context
  };
  LCIU_free(ctx);
  return request;
}

#include "pool.h"
#include "packet.h"
#include "proto.h"
#endif
