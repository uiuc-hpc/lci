#ifndef SERVER_H_
#define SERVER_H_

//struct LCID_server_opaque_t;
//typedef struct LCID_server_opaque_t* LCID_server_t;

/**
 * Data structure containing information of peers
 */

typedef uintptr_t LCID_mr_t;
typedef uint64_t LCID_rkey_t;
typedef uint32_t LCID_meta_t; // immediate data

/* Following functions are required to be implemented by LCI */

static inline void lc_serve_recv(LCI_device_t device, lc_packet* p,
                                 int rank, size_t length,
                                 LCII_proto_t proto);
static inline void lc_serve_rdma(LCII_proto_t proto);
static inline void lc_serve_send(void *ctx);

/* Following functions are required to be implemented by each server backend. */

void lc_server_init(LCI_device_t device, LCID_server_t* s);
void lc_server_finalize(LCID_server_t s);
static inline int lc_server_progress(LCID_server_t s);

static inline LCID_mr_t lc_server_rma_reg(LCID_server_t s, void* buf, size_t size);
static inline void lc_server_rma_dereg(LCID_mr_t mr);
static inline LCID_rkey_t lc_server_rma_rkey(LCID_mr_t mr);

static inline LCI_error_t lc_server_sends(LCID_server_t s, int rank, void* buf,
                                   size_t size, LCID_meta_t meta);
static inline LCI_error_t lc_server_send(LCID_server_t s, int rank, void* buf,
                                  size_t size, LCID_mr_t mr, LCID_meta_t meta,
                                  void* ctx);
static inline LCI_error_t lc_server_puts(LCID_server_t s, int rank, void* buf,
                                  size_t size, uintptr_t base, uint32_t offset,
                                  LCID_rkey_t rkey, uint32_t meta);
static inline LCI_error_t lc_server_put(LCID_server_t s, int rank, void* buf,
                                 size_t size, LCID_mr_t mr, uintptr_t base,
                                 uint32_t offset, LCID_rkey_t rkey,
                                 LCID_meta_t meta, void* ctx);

static inline int lc_server_recv_posted_num(LCID_server_t s);

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
