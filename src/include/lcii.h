#ifndef LCI_PRIV_H_
#define LCI_PRIV_H_

#include "lci.h"
#include "lciu.h"
#include "config.h"
#include "lcm_log.h"
#include "lcm_dequeue.h"
#include "pmi_wrapper.h"
#include "lcm_register.h"

struct LCID_server_opaque_t;
typedef struct LCID_server_opaque_t* LCID_server_t;

struct lc_packet;
typedef struct lc_packet lc_packet;

struct lc_pool;
typedef struct lc_pool lc_pool;

struct lc_hash;
typedef struct lc_hash lc_hash;

struct lc_req;
typedef struct lc_rep lc_rep;

struct lc_hash;
typedef struct lc_hash* LCI_mt_t;

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

struct LCI_device_s {
  LCID_server_t server;
  lc_pool* pkpool;
  LCI_mt_t mt;
  LCI_lbuffer_t heap;
};

struct LCI_plist_s {
  LCI_match_t match_type;     // matching type
  LCI_comp_type_t cmd_comp_type; // source-side completion type
  LCI_comp_type_t msg_comp_type; // destination-side completion type
  LCI_allocator_t allocator;  // dynamic allocator
};

struct LCI_endpoint_s {
  // Associated hardware context.
  LCI_device_t device;
  uint64_t property;

  // Associated software components.
  lc_pool* pkpool;
  LCI_mt_t mt;
  LCM_archive_t ctx_archive; // used for long message protocol

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
  // LCI_request_t fields, 45 bytes
  void* user_context;         // 8 bytes
  LCI_data_t data;            // 24 bytes
  LCI_data_type_t data_type;  // 4 bytes
  uint32_t rank;              // 4 bytes
  LCI_tag_t tag;              // 4 bytes
  // used by LCI internally
  LCI_comp_type_t comp_type;  // 4 bytes
  LCI_comp_t completion;      // 8 bytes
  bool is_dynamic;            // 1 byte
} LCII_context_t;

/**
 * Synchronizer, owned by the user.
 */
struct LCII_sync_t;
typedef struct LCII_sync_t LCII_sync_t;
LCI_error_t LCI_sync_signal(LCI_comp_t completion, LCII_context_t* ctx);

extern LCI_endpoint_t *LCI_ENDPOINTS;
extern int g_server_no_recv_packets;

// completion queue
static inline void LCII_queue_push(LCI_comp_t cq, LCII_context_t *ctx);
// matching table
LCI_error_t LCII_mt_init(LCI_mt_t* mt, uint32_t length);
LCI_error_t LCII_mt_free(LCI_mt_t* mt);
// device
void lc_env_init(int num_proc, int rank);
void lc_dev_init(LCI_device_t *device);
void lc_dev_finalize(LCI_device_t device);

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

// proto
/*
 * used by LCII_MAKE_PROTO (3 bits) for communication immediate data field
 * and LCII_make_key (2 bits, only use the first three entries) for the matching
 * table key. Take care when modify this enum type.
 */
typedef enum {
  LCI_MSG_SHORT,
  LCI_MSG_MEDIUM,
  LCI_MSG_LONG,
  LCI_MSG_RTS,
  LCI_MSG_RTR,
  LCI_MSG_RDMA_SHORT,
  LCI_MSG_RDMA_MEDIUM,
  LCI_MSG_RDMA_LONG,
} LCI_msg_type_t;
typedef uint32_t LCII_proto_t;
// 16 bits for tag, 13 bits for rgid, 3 bits for msg_type
#define LCII_MAKE_PROTO(rgid, msg_type, tag) (msg_type | (rgid << 3) | (tag << 16))
#define PROTO_GET_TYPE(proto) (proto & 0b0111)
#define PROTO_GET_RGID(proto) ((proto >> 3) & 0b01111111111111)
#define PROTO_GET_TAG(proto) ((proto >> 16) & 0xffff)
static inline uint64_t LCII_make_key(LCI_endpoint_t ep, int rank, LCI_tag_t tag,
                       LCI_msg_type_t msg_type);
// backend service
static inline void lc_ce_dispatch(LCII_context_t *ctx);
// rendezvous
static inline void LCII_handle_2sided_rts(LCI_endpoint_t ep, lc_packet* packet, LCII_context_t *long_ctx);
static inline void LCII_handle_2sided_rtr(LCI_endpoint_t ep, lc_packet* packet);
static inline void LCII_handle_2sided_writeImm(LCI_endpoint_t ep, uint64_t ctx_key);

#include "hashtable.h"
#include "cq.h"
#include "pool.h"
#include "packet.h"
#include "server/server.h"
#include "lcii_rdv.h"
#include "proto.h"
#endif
