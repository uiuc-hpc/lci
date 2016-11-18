#ifndef ISEND_H_
#define ISEND_H_

#include <mpi.h>

extern int mv_send_start, mv_send_end;
void proto_send_rdz(MPIV_Request* s);
void proto_send_short(const void* buffer, int size, int rank, int tag);

void mv_isend(const void* buf, size_t size, int rank, int tag, MPIV_Request* req) {
  if (size <= SHORT_MSG_SIZE) {
    req->type = REQ_SEND_SHORT;
    proto_send_short(buf, size, rank, tag);
  } else {
    new (req) MPIV_Request((void*)buf, size, rank, tag);
    req->type = REQ_SEND_LONG;
    proto_send_rdz(req);
  }
}

#endif
