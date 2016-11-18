#ifndef REQUEST_H_
#define REQUEST_H_

#include "mpiv.h"

enum RequestType {
  REQ_NULL,
  REQ_DONE,
  REQ_PENDING,
  REQ_RECV_SHORT,
  REQ_RECV_LONG,
  REQ_SEND_SHORT,
  REQ_SEND_LONG
};

struct MPIV_Request {
  inline MPIV_Request() : sync(NULL) {};
  inline MPIV_Request(int rank_, int tag_)
      : rank(rank_), tag(tag_), sync(NULL) {}
  inline MPIV_Request(void* buffer_, int size_, int rank_, int tag_)
      : buffer(buffer_),
        size(size_),
        rank(rank_),
        tag(tag_),
        sync(NULL) {};
  void* buffer;
  int size;
  int rank;
  int tag;
  mv_sync* sync;
  RequestType type;
} __attribute__((aligned(64)));

#endif
