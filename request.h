#ifndef REQUEST_H_
#define REQUEST_H_

struct mpiv_packet;

struct MPIV_Request {
  inline MPIV_Request(void* buffer_, int size_, int rank_, int tag_)
      : buffer(buffer_), size(size_), rank(rank_), tag(tag_), done_(false) {};
  fult_sync sync;
  void* buffer;
  int size;
  int rank;
  int tag;
  // mpiv_packet* packet;
  volatile bool done_;
} __attribute__((aligned(64)));

inline void MPIV_Wait(MPIV_Request& req) {
  if (!req.sync.has_ctx()) {
    while (!req.done_) {};
  } else {
    req.sync.wait();
  }
}

inline void MPIV_Signal(MPIV_Request* req) {
  req->done_ = true;
  if (req->sync.has_ctx()) {
    req->sync.signal();
  };
}

#endif
