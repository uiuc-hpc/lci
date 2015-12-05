#ifndef REQUEST_H_
#define REQUEST_H_

struct MPIV_Request {

   inline MPIV_Request(void* buffer_, int size_, int rank_, int tag_) :
          buffer(buffer_), size(size_), rank(rank_), tag(tag_) {
    };

    fult_sync sync;
    void* buffer;
    int size;
    int rank;
    int tag;
} __attribute__ ((aligned (64)));

bool MPIV_Progress();

inline void MPIV_Wait(MPIV_Request& req) {
    req.sync.wait();
}

#endif
