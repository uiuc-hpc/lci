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
  inline mv_ctx() : sync(NULL){};
  inline mv_ctx(int rank_, int tag_) : rank(rank_), tag(tag_), sync(NULL)
  {
  }
  inline mv_ctx(void* buffer_, int size_, int rank_, int tag_)
      : buffer(buffer_), size(size_), rank(rank_), tag(tag_), sync(NULL){};
  void* buffer;
  int size;
  int rank;
  int tag;
  mv_sync* sync;
  RequestType type;
  fcomplete complete;
} __attribute__((aligned(64)));

#endif
