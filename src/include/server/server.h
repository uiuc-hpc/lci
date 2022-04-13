#ifndef SERVER_H_
#define SERVER_H_

struct LCID_server_opaque_t;
typedef struct LCID_server_opaque_t* LCIS_server_t;

typedef struct LCIS_mr_t {
  // an opaque handler representing a dreg_entry (if LCI_USE_DREG is defined)
  // or a server-specific mr object
  uintptr_t mr_p;
  void * address;
  size_t length;
} LCIS_mr_t;

typedef uint64_t LCIS_rkey_t;
typedef uint32_t LCIS_meta_t; // immediate data
enum LCIS_opcode_t {
  LCII_OP_SEND,
  LCII_OP_RECV,
  LCII_OP_RDMA_WRITE,
};
typedef struct LCIS_cq_entry_t {
  enum LCIS_opcode_t opcode;
  int rank;
  void * ctx;
  size_t length;
  uint32_t imm_data;
} LCIS_cq_entry_t;

/* Following functions are required to be implemented by LCI */

static inline void lc_serve_recv(void* p, int rank, size_t length, uint32_t imm_data);
static inline void lc_serve_rdma(uint32_t imm_data);
static inline void lc_serve_send(void *ctx);

/* Following functions are required to be implemented by each server backend. */

void lc_server_init(LCI_device_t device, LCIS_server_t* s);
void lc_server_finalize(LCIS_server_t s);
static inline int LCID_poll_cq(LCIS_server_t s, LCIS_cq_entry_t *entry);

static inline LCIS_mr_t lc_server_rma_reg(LCIS_server_t s, void* buf, size_t size);
static inline void lc_server_rma_dereg(LCIS_mr_t mr);
static inline LCIS_rkey_t lc_server_rma_rkey(LCIS_mr_t mr);

static inline LCI_error_t lc_server_sends(LCIS_server_t s, int rank, void* buf,
                                          size_t size, LCIS_meta_t meta);
static inline LCI_error_t lc_server_send(LCIS_server_t s, int rank, void* buf,
                                         size_t size, LCIS_mr_t mr,
                                         LCIS_meta_t meta,
                                         void* ctx);
static inline LCI_error_t lc_server_puts(LCIS_server_t s, int rank, void* buf,
                                         size_t size, uintptr_t base,
                                         uint32_t offset, LCIS_rkey_t rkey,
                                         uint32_t meta);
static inline LCI_error_t lc_server_put(LCIS_server_t s, int rank, void* buf,
                                        size_t size, LCIS_mr_t mr, uintptr_t base,
                                        uint32_t offset,LCIS_rkey_t rkey,
                                        LCIS_meta_t meta, void* ctx);
static inline void lc_server_post_recv(LCIS_server_t s, void *buf,
                                       uint32_t size, LCIS_mr_t mr,
                                       void *ctx);

#ifdef LCI_USE_SERVER_OFI
#include "server_ofi.h"
#endif
#ifdef LCI_USE_SERVER_PSM
#include "erver_psm2.h"
#endif
#ifdef LCI_USE_SERVER_IBV
#include "server_ibv.h"
#endif

#endif
