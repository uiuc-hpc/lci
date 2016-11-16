#ifndef IRECV_H_
#define IRECV_H_

extern int mv_recv_start, mv_recv_end;

extern int mv_worker_id();

void proto_recv_rndz(void* buffer, int size, int rank, int tag,
                     MPIV_Request* s);

void mv_irecv(void* buffer, size_t size, int rank, int tag,
           MPIV_Request* req) {
  new (req) MPIV_Request(buffer, size, rank, tag);
  if (size <= SHORT_MSG_SIZE) 
    req->type = REQ_RECV_SHORT;
  else {
    req->type = REQ_RECV_LONG;
    proto_recv_rndz(buffer, size, rank, tag, req);
  }
}

#endif
