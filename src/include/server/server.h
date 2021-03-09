#ifndef SERVER_H_
#define SERVER_H_

#include "config.h"

struct lc_server;
typedef struct lc_server lc_server;

/**
 * Data structure containing information of peers
 */
struct lc_req;
typedef struct lc_rep lc_rep;

typedef uintptr_t LCID_mr_t;
typedef uint64_t LCID_rkey_t;
typedef void* LCID_addr_t;
typedef uint32_t LCID_meta_t; // immediate data

#define SERVER_COMMON \
  int id; \
  lc_pool* pkpool; \
  struct lc_rep* rep; \
  size_t recv_posted; \
  uintptr_t heap_addr; \

struct lc_rep {
  LCID_addr_t handle;
  int rank;
  intptr_t base;    /* starting address of the packet memory segment */
  LCID_rkey_t rkey; /* remote key to the packet memory segment */
};

extern volatile uint32_t lc_next_rdma_key;

/* Following functions are required to be implemented by LCI */

static inline void lc_serve_recv(lc_packet* p, uint32_t src_rank, size_t length,
                                 LCII_proto_t proto);
static inline void lc_serve_imm(lc_packet* p);
static inline void lc_serve_recv_rdma(lc_packet*, LCII_proto_type_t proto);
static inline void lc_serve_send(void *ctx);

/* Following functions are required to be implemented by each server backend. */

static inline void lc_server_init(int id, lc_server** dev);
static inline void lc_server_finalize(lc_server* s);
static inline int lc_server_progress(lc_server* s);

static inline LCID_mr_t lc_server_rma_reg(lc_server* s, void* buf, size_t size);
static inline void lc_server_rma_dereg(LCID_mr_t mr);
static inline LCID_rkey_t lc_server_rma_key(LCID_mr_t mr);

static inline void lc_server_sends(lc_server* s, LCID_addr_t dest, void* buf,
                                   size_t size, LCID_meta_t meta);
static inline void lc_server_send(lc_server* s, LCID_addr_t dest, void* buf,
                                  size_t size, LCID_mr_t mr, LCID_meta_t meta,
                                  void* ctx);
static inline void lc_server_puts(lc_server* s, LCID_addr_t dest, void* buf,
                                  size_t size, uintptr_t base, uint32_t offset,
                                  LCID_rkey_t rkey, uint32_t meta);
static inline void lc_server_put(lc_server* s, LCID_addr_t dest, void* buf,
                                 size_t size, LCID_mr_t mr, uintptr_t base,
                                 uint32_t offset, LCID_rkey_t rkey,
                                 LCID_meta_t meta, void* ctx);

static inline void lc_server_rma_rtr(lc_server* s, LCID_addr_t rep, void* buf,
                                     uintptr_t addr, uint64_t rkey, size_t size,
                                     uint32_t sid, lc_packet* p);

#ifdef LCI_USE_SERVER_OFI
#include "server_ofi.h"
#endif

#ifdef LCI_USE_SERVER_PSM
#include "server_psm2.h"
#endif

#ifdef LCI_USE_SERVER_IBV
#include "server_ibv.h"
#endif

#endif
