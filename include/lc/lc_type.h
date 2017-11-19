#ifndef LC_TYPE_H
#define LC_TYPE_H

#include "config.h"
#include <stdlib.h>
#include <stdint.h>
#include "lc/pool.h"
#include "lc/hashtable.h"

struct lc_struct;
typedef struct lc_struct lch;

typedef void (*lc_am_func_t)();
struct lc_ctx;
typedef struct lc_ctx lc_req;

typedef void* (*lc_alloc_fn)(void* ctx, size_t size);

struct lc_packet;
typedef struct lc_packet lc_packet;

typedef int16_t lc_qkey;
typedef int16_t lc_qtag;

typedef enum lc_status {
  LC_ERR_NOP = 0,
  LC_OK = 1,
} lc_status;

typedef void (*lc_fcb)(lch* mv, lc_req* req, lc_packet* p);

enum lc_req_state {
  LC_REQ_PEND = 0,
  LC_REQ_DONE = 1,
};

struct lc_ctx {
  void* buffer;
  size_t size;
  int rank;
  int tag;
  union {
    volatile enum lc_req_state type;
    volatile int int_type;
  };
  void* sync;
  lc_packet* packet;
} __attribute__((aligned(64)));

struct lc_rma_ctx {
  uint64_t addr;
  uint32_t rank;
  uint32_t rkey;
  uint32_t sid;
} __attribute__((aligned(64)));

typedef struct lc_rma_ctx lc_addr;

struct lc_pkt {
  void* _reserved_;
  void* buffer;
};

struct lc_server;

struct lc_struct {
  int me;
  int size;
  int ncores;
  struct lc_server* server;
  lc_pool* pkpool;
  lc_hash* tbl;

#ifndef USE_CCQ
  struct dequeue* queue;
#else
  lcrq_t* queue;
#endif
} __attribute__((aligned(64)));

#ifdef LC_USE_SERVER_OFI
#define SERVER_CONTEXT char __padding__[64 - 40];
#define ofi_server lc_server
#endif

#ifdef LC_USE_SERVER_IBV
#define SERVER_CONTEXT char __padding__[0];
#define ibv_server lc_server
#endif

#ifdef LC_USE_SERVER_PSM
#define SERVER_CONTEXT char __pandding__[64 - 40];
#define psm_server lc_server
#endif

struct __attribute__((packed)) packet_context {
  SERVER_CONTEXT;
  uint32_t from;
  uint32_t size;
  uint32_t tag;
  lc_req* req;
  uintptr_t rma_mem;
  uint8_t proto;
  uint8_t runtime;
  uint16_t poolid;
};

struct __attribute__((__packed__)) packet_rts {
  uintptr_t req;
  uintptr_t src_addr;
  size_t size;
};

struct __attribute__((__packed__)) packet_rtr {
  uintptr_t req;
  uintptr_t src_addr;
  size_t size;
  uintptr_t tgt_addr;
  uint32_t rkey;
  uint32_t comm_id;
};

struct __attribute__((__packed__)) packet_data {
  union {
    struct packet_rts rts;
    struct packet_rtr rtr;
    char buffer[0];
  };
};

struct __attribute__((packed)) lc_packet {
  struct packet_context context;
  struct packet_data data;
};

#endif
