#ifndef WAITALL_H_
#define WAITALL_H_

#include <assert.h>
#include <mpi.h>

extern int mpiv_send_start, mpiv_send_end;
void mpiv_complete_rndz(Packet* p, MPIV_Request* s);
void proto_send_rdz(MPIV_Request* s);
void proto_send_short(const void* buffer, int size, int rank, int tag);

inline void proto_req_recv_short_init(MPIV_Request* s) {
  mpiv_key key = mpiv_make_key(s->rank, s->tag);
  mpiv_value value;
  value.request = s;
  RequestType oldtype = s->type;
  if (!MPIV.tbl.insert(key, value)) {
    Packet* p_ctx = value.packet;
    memcpy(s->buffer, p_ctx->buffer(), s->size);
    MPIV.pkpool.ret_packet_to(p_ctx, worker_id());
    s->type = REQ_DONE;
    s->sync->count--;
  } else { 
    __sync_bool_compare_and_swap(&s->type, oldtype, REQ_PENDING);
  }
}

inline void proto_req_recv_long_init(MPIV_Request* req) {
  mpiv_key key = mpiv_make_key(req->rank, req->tag);
  mpiv_value value;
  RequestType oldtype = req->type;
  if (!MPIV.tbl.insert(key, value)) {
    req->type = REQ_DONE;
    req->sync->count--;
  } else {
    __sync_bool_compare_and_swap(&req->type, oldtype, REQ_PENDING);
  }
}

inline void proto_req_send_long_init(MPIV_Request* req) {
  mpiv_key key = mpiv_make_key(req->rank, (1 << 30) | req->tag);
  mpiv_value value;
  RequestType oldtype = req->type;
  if (!MPIV.tbl.insert(key, value)) {
    req->type = REQ_DONE;
    req->sync->count--;
  } else {
    __sync_bool_compare_and_swap(&req->type, oldtype, REQ_PENDING);
  }
}

inline bool init_sync(thread_sync& counter, int count, MPIV_Request* req) {
  bool ret = false;
  for (int i = 0; i < count; i++) {
    switch (req[i].type) {
      case REQ_NULL:
        break;
      case REQ_RECV_SHORT:
        req[i].sync = &counter;
        proto_req_recv_short_init(&req[i]);
        ret = true;
        break;
      case REQ_RECV_LONG:
        req[i].sync = &counter;
        proto_req_recv_long_init(&req[i]);
        ret = true;
        break;
      case REQ_SEND_LONG:
        req[i].sync = &counter;
        proto_req_send_long_init(&req[i]);
        ret = true;
        break;
      case REQ_DONE:
        counter.count--;
        ret = true;
        break;
      case REQ_PENDING:
        ret = true;
        break;
      default:
        counter.count--;
        ret = true;
        break;
    }
  }
  return ret;
}

// Assume short message for now.
void waitall(int count, MPIV_Request* req) {
  thread_sync counter(count);
  if (init_sync(counter, count, req)) {
    while (counter.count > 0) {
      thread_wait(&counter);
    }
    for (int i = 0; i < count; i++) {
      if (req[i].type == REQ_DONE) {
        req[i].type = REQ_NULL;
      } 
    }
  }
}

__thread thread_sync tls_counter;

void waitsome(int count, MPIV_Request* req, int* out_count, int* index) {
  *out_count = 0;
  thread_sync& counter = tls_counter;
  counter.count = 1;
  counter.thread = tlself.thread;
  if (init_sync(counter, count, req)) {
    while (counter.count > 0) {
      thread_wait(&counter);
      for (int i = 0; i < count; i++) {
        if (req[i].type == REQ_DONE) {
          index[*out_count] = i;
          *out_count = *out_count + 1;
          req[i].type = REQ_NULL;
          counter.count --;
        } 
      }
    }

    for (int i = 0; i < count; i++) {
      if (req[i].type == REQ_DONE) {
        index[*out_count] = i;
        *out_count = *out_count + 1;
        req[i].type = REQ_NULL;
      } 
    }
  }
}

#endif
