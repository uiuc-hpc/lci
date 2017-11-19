#include <stdlib.h>
#include <stdint.h>

struct lc_struct;
typedef struct lc_struct lch;

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

enum lc_req_state {
  LC_REQ_PEND = 0,
  LC_REQ_DONE = 1,
};

typedef void (*lc_fcb)(lch* mv, lc_req* req, lc_packet* p);

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

#define LC_PROTO_QUEUE   (0b0000000)
#define LC_PROTO_TAG     (0b0000001)
#define LC_PROTO_TGT     (0b0000010)

#define LC_PROTO_DATA    (0b0000100)
#define LC_PROTO_RTR     (0b0001000)
#define LC_PROTO_RTS     (0b0010000)
#define LC_PROTO_LONG    (0b0100000)
