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
    s->type = REQ_NULL;
    s->counter->count--;
  } else { 
    __sync_bool_compare_and_swap(&s->type, oldtype, REQ_PENDING);
  }
}

inline void proto_req_recv_long_init(MPIV_Request* req) {
  mpiv_key key = mpiv_make_key(req->rank, req->tag);
  mpiv_value value;
  RequestType oldtype = req->type;
  if (!MPIV.tbl.insert(key, value)) {
    req->type = REQ_NULL;
    req->counter->count--;
  } else {
    __sync_bool_compare_and_swap(&req->type, oldtype, REQ_PENDING);
  }
}

inline void proto_req_send_long_init(MPIV_Request* req) {
  mpiv_key key = mpiv_make_key(req->rank, (1 << 30) | req->tag);
  mpiv_value value;
  RequestType oldtype = req->type;
  if (!MPIV.tbl.insert(key, value)) {
    req->type = REQ_NULL;
    req->counter->count--;
  } else {
    __sync_bool_compare_and_swap(&req->type, oldtype, REQ_PENDING);
  }
}

inline bool init_sync(thread_counter& counter, int count, MPIV_Request* req) {
  bool ret = false;
  for (int i = 0; i < count; i++) {
    req[i].sync = tlself.thread;
    switch (req[i].type) {
      case REQ_NULL:
        break;
      case REQ_RECV_SHORT:
        req[i].counter = &counter;
        proto_req_recv_short_init(&req[i]);
        ret = true;
        break;
      case REQ_RECV_LONG:
        req[i].counter = &counter;
        proto_req_recv_long_init(&req[i]);
        ret = true;
        break;
      case REQ_SEND_LONG:
        req[i].counter = &counter;
        proto_req_send_long_init(&req[i]);
        ret = true;
        break;
      case REQ_DONE:
        req[i].counter = &counter;
        req[i].type = REQ_NULL;
        counter.count--;
        ret = true;
        break;
      case REQ_PENDING:
        ret = true;
        break;
      default:
        req[i].counter = &counter;
        counter.count--;
        ret = true;
        break;
    }
  }
  return ret;
}

// Assume short message for now.
void waitall(int count, MPIV_Request* req) {
  thread_counter counter(count);
  init_sync(counter, count, req);
  while (counter.count > 0)
    thread_wait(&counter);
}

void waitsome(int count, MPIV_Request* req, int* out_count, int* index) {
  *out_count = 0;
   thread_counter counter(1);
  if (init_sync(counter, count, req)) {
    while (counter.count > 0) {
      thread_wait(&counter);
      if (counter.count > 0) {
        for (int i = 0; i < count; i++) {
          if (!req[i].counter && req[i].type == REQ_DONE) {
            index[*out_count] = i;
            *out_count = *out_count + 1;
            req[i].type = REQ_NULL;
            counter.count--;
          }
        }
      }
    }

    for (int i = 0; i < count; i++) {
      if (req[i].counter && req[i].type == REQ_NULL) {
        index[*out_count] = i;
        *out_count = *out_count + 1;
      }
      req[i].counter = NULL;
    }
  }
}

#endif
