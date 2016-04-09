#ifndef REQUEST_H_
#define REQUEST_H_

struct mpiv_packet;

extern __thread fult* __fulting;

struct MPIV_Request {
  inline MPIV_Request(int rank_, int tag_)
      : rank(rank_), tag(tag_), sync(__fulting), done_(false) {}
  inline MPIV_Request(void* buffer_, int size_, int rank_, int tag_)
      : buffer(buffer_),
        size(size_),
        rank(rank_),
        tag(tag_),
        sync(__fulting),
        done_(false){};
  void* buffer;
  int size;
  int rank;
  int tag;
  fult* sync;
  volatile bool done_;
} __attribute__((aligned(64)));

inline void MPIV_Wait(MPIV_Request* req) {
  if (xunlikely(req->done_)) return;
  if (!req->sync) {
    while (!req->done_) {
      sched_yield();
    };
  } else {
    req->sync->wait();
  }
}

void MPIV_Waitall(int count, MPIV_Request request[], MPI_Status*) {
  for (int i=0; i<count; i++) {
    MPIV_Wait(&request[i]);
  }
}


inline void MPIV_Signal(MPIV_Request* req) {
  // Copy this first, because after done_ = true the request could be replaced.
  fult* sync = req->sync;
  req->done_ = true;
  // Since we have a copy, should be able to resume safely.
  if (sync) { sync->resume(); };
}

#endif
