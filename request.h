#ifndef REQUEST_H_
#define REQUEST_H_

#include "mpiv.h"

struct mpiv_packet;

extern __thread thread __fulting;

struct MPIV_Request {
  inline MPIV_Request(): counter(NULL) {};
  inline MPIV_Request(int rank_, int tag_)
      : rank(rank_), tag(tag_), sync(__fulting), done_(false), counter(NULL) {}
  inline MPIV_Request(void* buffer_, int size_, int rank_, int tag_)
      : buffer(buffer_),
        size(size_),
        rank(rank_),
        tag(tag_),
        sync(__fulting),
        done_(false), counter(NULL) {};
  void* buffer;
  int size;
  int rank;
  int tag;
  thread sync;
  bool done_;
  std::atomic<int>* counter;
} __attribute__((aligned(64)));

inline void MPIV_Wait(MPIV_Request* req) {
  if (xunlikely(req->done_)) return;
  // printf(">>> WAIT: rank %d tag %d sync id %d origin %d\n", req->rank, req->tag, req->sync->id(), req->sync->get_worker_id());
  req->sync->wait(req->done_);
}

inline void MPIV_Signal(MPIV_Request* req) {
  // printf(">>> rank %d tag %d sync id %d origin %d\n", req->rank, req->tag, req->sync->id(), req->sync->get_worker_id());
  req->sync->resume(req->done_);
}

#endif
