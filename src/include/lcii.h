#ifndef LCI_PRIV_H_
#define LCI_PRIV_H_

#include "lci.h"
#include "config.h"
#include "log.h"
#include "cq.h"
#include "lcii_register.h"

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

struct LCI_mt_s;
typedef struct LCI_mt_s* LCI_mt_t;

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
  LCI_comp_type_t cmd_comp_type; // source-side completion type
  LCI_comp_type_t msg_comp_type; // destination-side completion type
  LCI_allocator_t allocator;  // dynamic allocator
};

struct LCI_endpoint_s {
  // Associated hardware context.
  lc_server* server;
  uint64_t property;

  // Associated software components.
  lc_pool* pkpool;
  lc_rep* rep;
  LCI_mt_t mt;
  LCII_register_t ctx_reg; // used for long message protocol

  // user-defined components
  LCI_match_t match_type;     // matching type (tag/ranktag)
  LCI_comp_type_t cmd_comp_type; // command-port completion type
  LCI_comp_type_t msg_comp_type; // message-port completion type
  LCI_allocator_t allocator;  // dynamic allocator @note redundant for now

  volatile int completed;

  int gid;
};

/**
 * Internal context structure, Used by asynchronous operations to pass
 * information between initialization phase and completion phase.
 * (1) for issue_send->send_completion, go through user_context field of backends
 * (2) for issue_recv->recv_completion, go through the matching table
 * (3) for issue_recvl->issue_rtr->rdma_completion, go through the matching table and register
 */
typedef struct {
  // LCI_request_t fields, 44 bytes
  void* user_context;         // 8 bytes
  LCI_data_t data;            // 24 bytes
  LCI_data_type_t data_type;  // 4 bytes
  uint32_t rank;              // 4 bytes
  LCI_tag_t tag;              // 4 bytes
  // used by LCI internally
  LCI_comp_type_t comp_type;  // 4 bytes
  LCI_comp_t completion;      // 8 bytes
  int reg_key;                // 4 bytes
} LCII_context_t;

/**
 * Synchronizer, owned by the user.
 */
struct LCII_sync_t;
typedef struct LCII_sync_t LCII_sync_t;
LCI_error_t LCI_sync_signal(LCI_comp_t completion, LCII_context_t* ctx);

extern lc_server** LCI_DEVICES;
extern LCI_plist_t* LCI_PLISTS;
extern LCI_endpoint_t* LCI_ENDPOINTS;
extern int lcg_deadlock;

// matching table
LCI_error_t LCII_mt_init(LCI_mt_t* mt, uint32_t length);
LCI_error_t LCII_mt_free(LCI_mt_t* mt);
// device
void lc_env_init(int num_proc, int rank);
void lc_dev_init(int id, lc_server** dev, LCI_plist_t *plist);
void lc_dev_finalize(lc_server* dev);

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

#include "hashtable.h"
#include "pool.h"
#include "packet.h"
#include "proto.h"
#endif
