#ifndef LCI_PRIV_H_
#define LCI_PRIV_H_

#include "lcii_config.h"
#include "lci.h"
#include "log/lcm_log.h"
#include "sys/lciu.h"
#include "performance_counter.h"
#include "datastructure/lcm_dequeue.h"
#include "datastructure/lcm_aqueue.h"
#include "datastructure/lcm_archive.h"
#include "pmi_wrapper.h"
#include "backend/server.h"
#include "runtime/rcache/lcii_rcache.h"
#include "backlog_queue.h"

struct LCII_packet_t;
typedef struct LCII_packet_t LCII_packet_t;

struct LCII_pool_t;
typedef struct LCII_pool_t LCII_pool_t;

struct LCM_hashtable_t;
typedef struct LCM_hashtable_t LCM_hashtable_t;
typedef struct LCM_hashtable_t* LCI_mt_t;

struct LCI_device_s {
  // the following will not be changed after initialization
  LCIS_server_t server;  // 8B
  LCII_pool_t* pkpool;   // 8B
  LCI_mt_t mt;           // 8B
  LCII_rcache_t rcache;  // 8B
  LCI_lbuffer_t heap;    // 24B
  char padding0[64 - sizeof(LCIS_server_t) - sizeof(LCII_pool_t*) -
                sizeof(LCI_mt_t) - sizeof(LCII_rcache_t*) -
                sizeof(LCI_lbuffer_t)];
  // the following will be changed locally by a progress thread
  int recv_posted;  // 4B
  char padding1[64 - sizeof(int)];
  // the following is shared by both progress threads and worker threads
  LCM_archive_t ctx_archive;  // used for long message protocol
  char padding2[64 - (sizeof(LCM_archive_t) % 64)];
  LCII_backlog_queue_t bq;
  LCIU_spinlock_t bq_spinlock;
  char padding3[64 - ((sizeof(LCII_backlog_queue_t) + sizeof(LCIU_spinlock_t)) %
                      64)];
} __attribute__((aligned(64)));

struct LCI_plist_s {
  LCI_match_t match_type;         // matching type
  LCI_comp_type_t cmd_comp_type;  // source-side completion type
  LCI_comp_type_t msg_comp_type;  // destination-side completion type
  LCI_comp_t default_comp;        // default comp for one-sided communication
};

struct LCI_endpoint_s {
  // Associated hardware context.
  LCI_device_t device;
  uint64_t property;

  // Associated software components.
  LCII_pool_t* pkpool;
  LCI_mt_t mt;
  // used for the rendezvous protocol
  LCM_archive_t* ctx_archive_p;
  LCII_backlog_queue_t* bq_p;
  LCIU_spinlock_t* bq_spinlock_p;

  // user-defined components
  LCI_match_t match_type;         // matching type (tag/ranktag)
  LCI_comp_type_t cmd_comp_type;  // command-port completion type
  LCI_comp_type_t msg_comp_type;  // message-port completion type
  LCI_comp_t default_comp;        // default comp for one-sided communication

  volatile int completed;

  int gid;
};

struct LCII_mr_t {
  LCI_device_t device;
  void* region;
  LCIS_mr_t mr;
};
typedef struct LCII_mr_t LCII_mr_t;

/**
 * Internal context structure, Used by asynchronous operations to pass
 * information between initialization phase and completion phase.
 * (1) for issue_send->send_completion, go through user_context field of
 * backends (2) for issue_recv->recv_completion, go through the matching table
 * (3) for issue_recvl->issue_rtr->rdma_completion, go through the matching
 * table and register It is also used by rendezvous protocol to pass information
 * between different steps (1) for sending_RTS->sending_WriteImm (long,iovec),
 * go through the send_ctx field of the RTS and RTR messages (2) for
 * sending_RTR->complet_WriteImm (long), go through the ctx_archive. Its key is
 *     passed by the recv_ctx_key field of RTR messages/as meta data of
 * WriteImm. (3) for recving_RTR->complet_All_Write (iovec), go through the
 * ctx_archiveMulti. (4) for sending_RTR->recving_FIN (iovec), go through the
 * recv_ctx field of RTR and FIN messages.
 */
typedef struct {
  // used by LCI internally
  uint32_t comp_attr;  // 4 bytes
  // LCI_request_t fields, 52 bytes
  LCI_data_type_t data_type;  // 4 bytes
  void* user_context;         // 8 bytes
  LCI_data_t data;            // 32 bytes
  uint32_t rank;              // 4 bytes
  LCI_tag_t tag;              // 4 bytes
  // used by LCI internally
  LCI_comp_t completion;  // 8 bytes
} LCII_context_t __attribute__((aligned(64)));
/**
 * comp_type: user-defined comp_type
 * free_packet: free mbuffer as a packet.
 * dereg: deregister the lbuffer.
 * extended: extended context for iovec.
 */
#define LCII_initilize_comp_attr(comp_attr) comp_attr = 0
#define LCII_comp_attr_set_comp_type(comp_attr, comp_type) \
  comp_attr = LCIU_set_bits32(comp_attr, comp_type, 3, 0)
#define LCII_comp_attr_set_free_packet(comp_attr, free_packet) \
  comp_attr = LCIU_set_bits32(comp_attr, free_packet, 1, 3)
#define LCII_comp_attr_set_dereg(comp_attr, dereg) \
  comp_attr = LCIU_set_bits32(comp_attr, dereg, 1, 4)
#define LCII_comp_attr_set_extended(comp_attr, flag) \
  comp_attr = LCIU_set_bits32(comp_attr, flag, 1, 5)
#define LCII_comp_attr_get_comp_type(comp_attr) LCIU_get_bits32(comp_attr, 3, 0)
#define LCII_comp_attr_get_free_packet(comp_attr) \
  LCIU_get_bits32(comp_attr, 1, 3)
#define LCII_comp_attr_get_dereg(comp_attr) LCIU_get_bits32(comp_attr, 1, 4)
#define LCII_comp_attr_get_extended(comp_attr) LCIU_get_bits32(comp_attr, 1, 5)

// Extended context for iovec
typedef struct {
  uint32_t comp_attr;       // 4 bytes
  int signal_count;         // 4 bytes
  int signal_expected;      // 4 bytes
  uintptr_t recv_ctx;       // 8 bytes
  LCI_endpoint_t ep;        // 8 bytes
  LCII_context_t* context;  // 8 bytes
} LCII_extended_context_t __attribute__((aligned(64)));

/**
 * Synchronizer, owned by the user.
 */
struct LCII_sync_t;
typedef struct LCII_sync_t LCII_sync_t;
LCI_error_t LCII_sync_signal(LCI_comp_t completion, LCII_context_t* ctx);

extern LCI_endpoint_t* LCI_ENDPOINTS;

// completion queue
static inline void LCII_queue_push(LCI_comp_t cq, LCII_context_t* ctx);
// matching table
LCI_error_t LCII_mt_init(LCI_mt_t* mt, uint32_t length);
LCI_error_t LCII_mt_free(LCI_mt_t* mt);
void LCII_env_init(int num_proc, int rank);

static inline LCI_request_t LCII_ctx2req(LCII_context_t* ctx)
{
  LCI_request_t request = {.flag = LCI_OK,
                           .rank = ctx->rank,
                           .tag = ctx->tag,
                           .type = ctx->data_type,
                           .data = ctx->data,
                           .user_context = ctx->user_context};
  LCIU_free(ctx);
  return request;
}

// proto
/*
 * used by LCII_MAKE_PROTO (4 bits) for communication immediate data field
 * and LCII_make_key (2 bits, only use the first three entries) for the matching
 * table key. Take care when modify this enum type.
 */
typedef enum {
  LCI_MSG_SHORT,
  LCI_MSG_MEDIUM,
  LCI_MSG_LONG,
  LCI_MSG_RTS,
  LCI_MSG_RTR,
  LCI_MSG_FIN,
  LCI_MSG_RDMA_SHORT,
  LCI_MSG_RDMA_MEDIUM,
  LCI_MSG_RDMA_LONG,
  LCI_MSG_IOVEC,
} LCI_msg_type_t;
typedef uint32_t LCII_proto_t;
// 16 bits for tag, 12 bits for rgid, 4 bits for msg_type
#define LCII_MAKE_PROTO(rgid, msg_type, tag) \
  (msg_type | (rgid << 4) | (tag << 16))
#define PROTO_GET_TYPE(proto) (proto & 0b01111)
#define PROTO_GET_RGID(proto) ((proto >> 4) & 0b0111111111111)
#define PROTO_GET_TAG(proto) ((proto >> 16) & 0xffff)
static inline uint64_t LCII_make_key(LCI_endpoint_t ep, int rank, LCI_tag_t tag,
                                     LCI_msg_type_t msg_type);
// backend service
static inline void lc_ce_dispatch(LCII_context_t* ctx);
// rendezvous
static inline void LCII_handle_2sided_rts(LCI_endpoint_t ep,
                                          LCII_packet_t* packet,
                                          LCII_context_t* long_ctx);
static inline void LCII_handle_2sided_rtr(LCI_endpoint_t ep,
                                          LCII_packet_t* packet);
static inline void LCII_handle_2sided_writeImm(LCI_endpoint_t ep,
                                               uint64_t ctx_key);

// monitor thread
void LCII_monitor_thread_init();
void LCII_monitor_thread_fina();

#include "datastructure/lcm_hashtable.h"
#include "runtime/completion/cq.h"
#include "packet_pool.h"
#include "packet.h"
#include "rendezvous.h"
#include "protocol.h"
#endif
