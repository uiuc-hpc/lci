/**
 * @file lc.h
 * @author Hoang-Vu Dang (danghvu@gmail.com)
 * @brief Header file for all MPIv code.
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

enum lc_wr_state {
  LC_REQ_PEND = 0,
  LC_REQ_DONE = 1,
};

enum lc_wr_type {
  WR_PROD = 0,
  WR_CONS = 1,
};

enum lc_sig_type {
  SIG_NONE = 0,
  SIG_CQ,
  SIG_WAKE,
  SIG_CNTRS,
  SIG_HANDL,
  SIG_GLOBAL
};

enum lc_data_type {
  DAT_EXPL = 0,
  DAT_ALLOC,
  DAT_TAG 
};

typedef enum lc_status {
  LC_OK = 0,
  LC_ERR_RETRY,
  LC_ERR_FATAL,
} lc_status;

typedef uint32_t lc_id;
typedef uint32_t lc_eid;
typedef uint32_t lc_gid;
typedef uint32_t lc_alloc_id;

// MPI tag is only upto 64K, it can be piggy-backed as well.
typedef uint16_t lc_tag;

// These using 1 byte since they need to be limitted,
// so that can be offloaded in the future.
typedef uint8_t lc_queue_id;
typedef uint8_t lc_sync_id; 
typedef uint8_t lc_handl_id; 
typedef uint8_t lc_global_id;

typedef struct lc_data {
  enum lc_data_type type;
  union {
    // explicit data.
    struct {
      void* addr;
      size_t size;
    };
    // allocator.
    struct {
      lc_alloc_id alloc_id;
      void* alloc_ctx;
    };
    // tag.
    lc_tag tag_val;
  };
} lc_data;

typedef struct lc_sig {
  enum lc_sig_type type;
  union {
    lc_id id;
    lc_queue_id q_id;
    lc_sync_id  s_id;
    lc_handl_id h_id;
    lc_global_id g_id;
  };
} lc_sig;

typedef struct lc_wr {
  enum lc_wr_type type;
  struct {
    struct lc_sig local_sig;
    struct lc_sig remote_sig;
    struct lc_data source_data;
    struct lc_data target_data;
    lc_id source;
    lc_id target;
  };
} lc_wr;

typedef struct lc_req {
  // This flag is going to be set when the communication is done.
  // It is going to be set by the communication serve most likely
  // so we are going to align it to avoid false sharing.
  volatile int flag __attribute__((aligned(64)));

  // Additional fields here.
  void* buffer;
  int rank;
  int tag;
  size_t size;
} lc_req;

LC_EXPORT
lc_status lc_init();

LC_EXPORT
lc_status lc_finalize();

LC_EXPORT
lc_status lc_submit(struct lc_wr* wr, lc_req* req); 

LC_EXPORT
lc_status lc_ce_test(struct lc_sig* sig, lc_req* req); 

LC_EXPORT
lc_id lc_rank();

LC_EXPORT
lc_status lc_progress();

LC_EXPORT
lc_status lc_free(void* buf);

#ifdef __cplusplus
}
#endif

#endif
