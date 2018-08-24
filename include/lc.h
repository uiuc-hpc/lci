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


#define EP_AR_ALLOC (1<<1)
#define EP_AR_EXPL  (1<<2)

#define EP_CE_FLAG ((1<<1) << 4)
#define EP_CE_CQ   ((1<<2) << 4)
#define EP_CE_SYNC ((1<<3) << 4)
#define EP_CE_AM   ((1<<4) << 4)

#define LC_REQ_PEND 0
#define LC_REQ_DONE 1

typedef enum lc_status {
  LC_OK = 0,
  LC_ERR_RETRY,
  LC_ERR_FATAL,
} lc_status;

typedef void* (*lc_alloc_fn)(void*, size_t);
typedef void (*lc_free_fn)(void*, void*);

typedef struct lci_rep* lc_rep;

// MPI tag is only upto 64K, it can be piggy-backed as well.
typedef union {
  struct {
    uint16_t trank;
    uint16_t tval;
  } tag;
  uint32_t val;
} lc_meta;

typedef struct lc_req {
  // This flag is going to be set when the communication is done.
  // It is going to be set by the communication serve most likely
  // so we are going to align it to avoid false sharing.
  volatile int flag;

  // Additional fields here.
  void* buffer;
  void* parent; // reserved field for internal used.
  void* rhandle;
  size_t size;
  int rank;
  lc_meta meta;
} lc_req;

typedef struct lci_ep* lc_ep;
typedef struct lci_dev* lc_dev;

LC_EXPORT
lc_status lc_init(int dev_id, long ar, long comp, lc_ep* ep);

LC_EXPORT
lc_status lc_ep_dup(int dev_id, long ar, long comp, lc_ep iep, lc_ep* ep);

LC_EXPORT
lc_status lc_sendm(lc_ep ep, int rep, void* src, size_t size, lc_meta tag, lc_req* req);

LC_EXPORT
lc_status lc_recvm(lc_ep ep, int rep, void* src, size_t size, lc_meta tag, lc_req* req);

LC_EXPORT
lc_status lc_sendl(lc_ep ep, int rep, void* src, size_t size, lc_meta tag, lc_req* req);

LC_EXPORT
lc_status lc_recvl(lc_ep ep, int rep, void* src, size_t size, lc_meta tag, lc_req* req);

LC_EXPORT
lc_status lc_putmd(lc_ep ep, int rep, void* src, size_t size, lc_meta tag, lc_req* req);

LC_EXPORT
lc_status lc_putld(lc_ep ep, int rep, void* src, size_t size, lc_meta tag, lc_req* req);

LC_EXPORT
lc_status lc_cq_popval(lc_ep ep, lc_req* req);

LC_EXPORT
lc_status lc_cq_popref(lc_ep ep, lc_req** req);

LC_EXPORT
lc_status lc_cq_reqfree(lc_ep ep, lc_req* req);

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
lc_status lc_free(lc_ep, void* buf);

LC_EXPORT
lc_status lc_ep_set_alloc(lc_ep ep, lc_alloc_fn alloc, lc_free_fn free, void* ctx);

LC_EXPORT
void lc_pm_barrier();

#ifdef __cplusplus
}
#endif

#endif
