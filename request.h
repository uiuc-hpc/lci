#ifndef REQUEST_H_
#define REQUEST_H_

struct mpiv_packet;

extern __thread fult* __fulting;

struct alignas(64) MPIV_Request {
  inline MPIV_Request(int rank_, int tag_) : rank(rank_), tag(tag_),
      sync(__fulting), done_(false) {}
  inline MPIV_Request(void* buffer_, int size_, int rank_, int tag_)
      : buffer(buffer_), size(size_), rank(rank_), tag(tag_), 
      sync(__fulting), done_(false) {};
  void* buffer;
  int size;
  int rank;
  int tag;
  fult* sync;
  volatile bool done_;
};

inline void MPIV_Wait(MPIV_Request* req) {
  if (!req->sync) {
    while (!req->done_) { sched_yield(); };
  } else {
    req->sync->wait();
  }
}

inline void MPIV_Signal(MPIV_Request* req) {
  req->done_ = true;
  if (req->sync) {
    req->sync->resume();
  };
}

#endif
