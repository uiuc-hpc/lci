#ifndef REQUEST_H_
#define REQUEST_H_

enum RequestType {
  REQ_NULL,
  REQ_DONE,
  REQ_PENDING,
  REQ_POSTED,
  REQ_RECV_SHORT,
  REQ_RECV_LONG,
  REQ_SEND_SHORT,
  REQ_SEND_LONG
};

typedef void(*fcomplete)(mv_engine* mv, mv_ctx* ctx, mv_sync* sync);

struct mv_ctx {
  void* buffer;
  int size;
  int rank;
  int tag;
  mv_sync* sync;
  enum RequestType type;
  fcomplete complete;
} __attribute__((aligned(64)));

#endif
