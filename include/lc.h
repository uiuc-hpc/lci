/**
 * @file lc.h
 * @author Hoang-Vu Dang (danghvu@gmail.com)
 * @brief Header file for all LCI code.
 *
 */

#ifndef LC_H_
#define LC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>
#include "lc/macro.h"
#include "thread.h"

typedef enum lc_status {
  LC_OK = 0,
  LC_ERR_RETRY,
  LC_ERR_FATAL,
} lc_status;

typedef enum lc_ep_addr {
  EP_AR_ALLOC = 1<<1,
  EP_AR_EXPL  = 1<<2,
} lc_ep_addr;

typedef enum lc_ep_ce {
  EP_CE_NULL = 0,
  EP_CE_SYNC = ((1<<1) << 4),
  EP_CE_CQ   = ((1<<2) << 4),
  EP_CE_AM   = ((1<<3) << 4),
} lc_ep_ce;

struct lc_ep_desc {
  lc_ep_addr addr;
  lc_ep_ce   ce;
} __attribute__((packed));

typedef uint32_t lc_meta;

struct lc_req {
  lc_sync sync;
  void* buffer;
  void* parent; // reserved field for internal used.
  void* rhandle;
  size_t size;
  int rank;
  lc_meta meta;
} __attribute__((packed, aligned(64)));

extern int lcg_nep;
extern int lcg_size;
extern int lcg_rank;
extern int lcg_page_size;

typedef struct lc_req lc_req;
typedef struct lc_ep_desc lc_ep_desc;

typedef struct lci_rep* lc_rep;
typedef struct lci_ep* lc_ep;
typedef struct lci_dev* lc_dev;

typedef void* (*lc_alloc_fn)(void* ctx, size_t malloc_size);
typedef void (*lc_free_fn)(void* ctx, void* buffer);
typedef void (*lc_handler_fn)(void* ctx, lc_req* pkt);

static const lc_ep_desc LC_EXPL_SYNC = {EP_AR_EXPL, EP_CE_SYNC};
static const lc_ep_desc LC_EXPL_CQ   = {EP_AR_EXPL, EP_CE_CQ};
static const lc_ep_desc LC_EXPL_AM   = {EP_AR_EXPL, EP_CE_AM};
static const lc_ep_desc LC_ALLOC_CQ  = {EP_AR_ALLOC, EP_CE_CQ};

LC_EXPORT
lc_status lc_init(int dev_id, lc_ep_desc desc, lc_ep* ep);

LC_EXPORT
lc_status lc_ep_dup(int dev_id, lc_ep_desc desc, lc_ep iep, lc_ep* ep);

LC_EXPORT
size_t lc_max_short(int dev_id);

LC_EXPORT
size_t lc_max_medium(int dev_id);

/* Short */
LC_EXPORT
lc_status lc_send(void* src, size_t size, int rank, lc_meta tag, lc_ep ep, lc_sync*);

LC_EXPORT
lc_status lc_sends(void* src, size_t size, int rank, lc_meta tag, lc_ep ep);

LC_EXPORT
lc_status lc_putsd(void* src, size_t size, int rank, lc_meta tag, lc_ep ep);

LC_EXPORT
lc_status lc_puts(void* src, size_t size, int rank, uintptr_t dst, lc_ep ep);

LC_EXPORT
lc_status lc_putss(void* src, size_t size, int rank, uintptr_t dst, lc_meta meta, lc_ep ep);

/* Medium */
LC_EXPORT
lc_status lc_sendm(void* src, size_t size, int rank, lc_meta tag, lc_ep ep);

LC_EXPORT
lc_status lc_putmd(void* src, size_t size, int rank, lc_meta tag, lc_ep ep);

LC_EXPORT
lc_status lc_putm(void* src, size_t size, int rank, uintptr_t dst, lc_ep ep);

LC_EXPORT
lc_status lc_putms(void* src, size_t size, int rank, uintptr_t dst, lc_meta tag, lc_ep ep);

/* Long */
LC_EXPORT
lc_status lc_sendl(void* src, size_t size, int rank, lc_meta tag, lc_ep ep, lc_sync*);

LC_EXPORT
lc_status lc_putld(void* src, size_t size, int rank, lc_meta tag, lc_ep ep, lc_sync*);

LC_EXPORT
lc_status lc_putl(void* src, size_t size, int rank, uintptr_t dst, lc_ep ep, lc_sync*);

LC_EXPORT
lc_status lc_putls(void* src, size_t size, int rank, uintptr_t dst, lc_meta meta, lc_ep ep, lc_sync*);

/* Receive */
LC_EXPORT
lc_status lc_recv(void* src, size_t size, int rank, lc_meta tag, lc_ep ep, lc_req* req);

LC_EXPORT
lc_status lc_recvm(void* src, size_t size, int rank, lc_meta tag, lc_ep ep, lc_req* req);

LC_EXPORT
lc_status lc_recvl(void* src, size_t size, int rank, lc_meta tag, lc_ep ep, lc_req* req);

LC_EXPORT
lc_status lc_cq_pop(lc_ep ep, lc_req** req);

LC_EXPORT
lc_status lc_cq_reqfree(lc_ep ep, lc_req* req);

LC_EXPORT
lc_status lc_free(lc_ep ep, void* buf);

LC_EXPORT
void lc_get_proc_num(int *rank);

LC_EXPORT
void lc_get_num_proc(int *size);

LC_EXPORT
lc_status lc_finalize();

LC_EXPORT
int lc_progress(int);

LC_EXPORT
int lc_progress_t(int);

LC_EXPORT
int lc_progress_q(int);

LC_EXPORT
lc_status lc_ep_set_alloc(lc_ep ep, lc_alloc_fn alloc, lc_free_fn free, void* ctx);

LC_EXPORT
lc_status lc_ep_set_handler(lc_ep ep, lc_handler_fn handler, void* ctx);

LC_EXPORT
lc_status lc_ep_get_baseaddr(lc_ep, size_t size, uintptr_t* addr);

LC_EXPORT
void lc_pm_barrier();

#ifdef __cplusplus
}
#endif

#endif
