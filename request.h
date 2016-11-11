#ifndef REQUEST_H_
#define REQUEST_H_

#include "mpiv.h"

namespace mpiv {

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
  inline MPIV_Request() : sync(NULL), counter(NULL) {};
  inline MPIV_Request(int rank_, int tag_)
      : rank(rank_), tag(tag_), sync(NULL), counter(NULL) {}
  inline MPIV_Request(void* buffer_, int size_, int rank_, int tag_)
      : buffer(buffer_),
        size(size_),
        rank(rank_),
        tag(tag_),
        sync(NULL), counter(NULL) {};
  void* buffer;
  int size;
  int rank;
  int tag;
  thread_sync* sync;
  thread_counter* counter;
  volatile RequestType type;
} __attribute__((aligned(64)));

inline void MPIV_Wait(MPIV_Request* req) {
  req->sync = tlself.thread;
  thread_wait(req->sync);
}

inline void MPIV_Signal(MPIV_Request* req) {
  if (xunlikely(req->counter != NULL))
    thread_signal(req->counter);
  else
    thread_signal(req->sync);
}

};  // namespace mpiv.

using MPIV_Request = mpiv::MPIV_Request;

#endif
