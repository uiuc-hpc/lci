#ifndef SERVER_H_
#define SERVER_H_

struct LCID_server_opaque_t;
typedef struct LCID_server_opaque_t* LCIS_server_t;

typedef uint64_t LCIS_offset_t;

typedef struct LCIS_mr_t {
  // an opaque handler representing a dreg_entry (if env LCI_USE_DREG is 1)
  // or a server-specific mr object
  void *mr_p;
  void *address;
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

static inline void LCIS_serve_recv(void* p, int rank, size_t length, uint32_t imm_data);
static inline void LCIS_serve_rdma(uint32_t imm_data);
static inline void LCIS_serve_send(void *ctx);

/* Following functions are required to be implemented by each server backend. */

void LCISD_init(LCI_device_t device, LCIS_server_t* s);
void LCISD_finalize(LCIS_server_t s);
static inline int LCISD_poll_cq(LCIS_server_t s, LCIS_cq_entry_t *entry);

static inline LCIS_mr_t LCISD_rma_reg(LCIS_server_t s, void* buf, size_t size);
static inline void LCISD_rma_dereg(LCIS_mr_t mr);
static inline LCIS_rkey_t LCISD_rma_rkey(LCIS_mr_t mr);

static inline LCI_error_t LCISD_post_sends(LCIS_server_t s, int rank, void* buf,
                                      size_t size, LCIS_meta_t meta);
static inline LCI_error_t LCISD_post_send(LCIS_server_t s, int rank, void* buf,
                                     size_t size, LCIS_mr_t mr,
                                     LCIS_meta_t meta, void* ctx);
static inline LCI_error_t LCISD_post_puts(LCIS_server_t s, int rank, void* buf,
                                     size_t size, uintptr_t base,
                                     LCIS_offset_t offset, LCIS_rkey_t rkey);
static inline LCI_error_t LCISD_post_put(LCIS_server_t s, int rank, void* buf,
                                    size_t size, LCIS_mr_t mr, uintptr_t base,
                                    LCIS_offset_t offset, LCIS_rkey_t rkey,
                                    void* ctx);
static inline LCI_error_t LCISD_post_putImms(LCIS_server_t s, int rank, void* buf,
                                        size_t size, uintptr_t base,
                                        LCIS_offset_t offset, LCIS_rkey_t rkey,
                                        uint32_t meta);
static inline LCI_error_t LCISD_post_putImm(LCIS_server_t s, int rank, void* buf,
                                       size_t size, LCIS_mr_t mr,
                                       uintptr_t base, LCIS_offset_t offset,
                                       LCIS_rkey_t rkey, LCIS_meta_t meta,
                                       void* ctx);
static inline void LCISD_post_recv(LCIS_server_t s, void *buf, uint32_t size,
                                   LCIS_mr_t mr, void *ctx);

#ifdef LCI_USE_SERVER_OFI
#include "server_ofi.h"
#endif
#ifdef LCI_USE_SERVER_IBV
#include "server_ibv.h"
#endif

/* Wrapper functions */
static inline void LCIS_init(LCI_device_t device, LCIS_server_t* s) {
  LCISD_init(device, s);
}

static inline void LCIS_finalize(LCIS_server_t s) {
  LCISD_finalize(s);
}

static inline int LCIS_poll_cq(LCIS_server_t s, LCIS_cq_entry_t *entry) {
  return LCISD_poll_cq(s, entry);
}

static inline LCIS_rkey_t LCIS_rma_rkey(LCIS_mr_t mr) {
  return LCISD_rma_rkey(mr);
}
static inline LCIS_mr_t LCIS_rma_reg(LCIS_server_t s, void* buf, size_t size) {
  LCIS_mr_t mr = LCISD_rma_reg(s, buf, size);
  LCM_DBG_Log(LCM_LOG_DEBUG, "server-reg", "LCIS_rma_reg: mr %p buf %p size %lu rkey %lu\n",
              mr.mr_p, buf, size, LCISD_rma_rkey(mr));
  return mr;
}
static inline void LCIS_rma_dereg(LCIS_mr_t mr) {
  LCM_DBG_Log(LCM_LOG_DEBUG, "server-reg", "LCIS_rma_dereg: mr %p buf %p size %lu rkey %lu\n",
              mr.mr_p, mr.address, mr.length, LCISD_rma_rkey(mr));
  LCISD_rma_dereg(mr);
}

static inline LCI_error_t LCIS_post_sends(LCIS_server_t s, int rank, void* buf,
                                          size_t size, LCIS_meta_t meta) {
  LCM_DBG_Log(LCM_LOG_DEBUG, "server", "LCIS_post_sends: rank %d buf %p size %lu meta %d\n",
                      rank, buf, size, meta);
  return LCISD_post_sends(s, rank, buf, size, meta);
}
static inline LCI_error_t LCIS_post_send(LCIS_server_t s, int rank, void* buf,
                                         size_t size, LCIS_mr_t mr,
                                         LCIS_meta_t meta, void* ctx) {
  LCM_DBG_Log(LCM_LOG_DEBUG, "server", "LCIS_post_send: rank %d buf %p size %lu mr %p meta %d ctx %p\n",
                      rank, buf, size, mr.mr_p, meta, ctx);
  return LCISD_post_send(s, rank, buf, size, mr, meta, ctx);
}
static inline LCI_error_t LCIS_post_puts(LCIS_server_t s, int rank, void* buf,
                                         size_t size, uintptr_t base,
                                         LCIS_offset_t offset, LCIS_rkey_t rkey) {
  LCM_DBG_Log(LCM_LOG_DEBUG, "server", "LCIS_post_puts: rank %d buf %p size %lu base %p offset %lu "
                      "rkey %lu\n", rank, buf,
                      size, (void*) base, offset, rkey);
  return LCISD_post_puts(s, rank, buf, size, base, offset, rkey);
}
static inline LCI_error_t LCIS_post_put(LCIS_server_t s, int rank, void* buf,
                                        size_t size, LCIS_mr_t mr,
                                        uintptr_t base, LCIS_offset_t offset,
                                        LCIS_rkey_t rkey, void* ctx) {

  LCM_DBG_Log(LCM_LOG_DEBUG, "server", "LCIS_post_put: rank %d buf %p size %lu mr %p base %p "
                      "offset %lu rkey %lu ctx %p\n", rank, buf,
                      size, mr.mr_p, (void*) base, offset, rkey, ctx);
  return LCISD_post_put(s, rank, buf, size, mr, base, offset, rkey, ctx);
}
static inline LCI_error_t LCIS_post_putImms(LCIS_server_t s, int rank,
                                            void* buf, size_t size,
                                            uintptr_t base, LCIS_offset_t offset,
                                            LCIS_rkey_t rkey, uint32_t meta) {

  LCM_DBG_Log(LCM_LOG_DEBUG, "server", "LCIS_post_putImms: rank %d buf %p size %lu base %p offset %lu "
                      "rkey %lu meta %d\n", rank, buf,
                      size, (void*) base, offset, rkey, meta);
  return LCISD_post_putImms(s, rank, buf, size, base, offset, rkey, meta);
}
static inline LCI_error_t LCIS_post_putImm(LCIS_server_t s, int rank,
                                           void* buf, size_t size,
                                           LCIS_mr_t mr, uintptr_t base,
                                           LCIS_offset_t offset,
                                           LCIS_rkey_t rkey,
                                           LCIS_meta_t meta, void* ctx) {

  LCM_DBG_Log(LCM_LOG_DEBUG, "server", "LCIS_post_putImm: rank %d buf %p size %lu mr %p base %p "
                      "offset %lu rkey %lu meta %u ctx %p\n", rank, buf,
                      size, mr.mr_p, (void*) base, offset, rkey, meta, ctx);
  return LCISD_post_putImm(s, rank, buf, size, mr, base, offset, rkey, meta, ctx);
}
static inline void LCIS_post_recv(LCIS_server_t s, void *buf,
                                  uint32_t size, LCIS_mr_t mr,
                                  void *ctx) {
  LCM_DBG_Log(LCM_LOG_DEBUG, "server", "LCIS_post_recv: buf %p size %u mr %p user_context %p\n",
                      buf, size, mr.mr_p, ctx);
  return LCISD_post_recv(s, buf, size, mr, ctx);
}

#endif
