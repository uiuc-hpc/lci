#ifndef WAITALL_H_
#define WAITALL_H_

#include <assert.h>
#include <mpi.h>

extern int mv_send_start, mv_send_end;
void mv_complete_rndz(packet* p, MPIV_Request* s);
void proto_send_rdz(MPIV_Request* s);
void proto_send_short(const void* buffer, int size, int rank, int tag);

inline void proto_req_recv_short_init(MPIV_Request* s) {
  mv_key key = mv_make_key(s->rank, s->tag);
  mv_value value = (mv_value) s;
  RequestType oldtype = s->type;
  if (!hash_insert(MPIV.tbl, key, value)) {
    packet* p_ctx = (packet*) value;
    memcpy(s->buffer, p_ctx->buffer(), s->size);
    mv_pp_free(MPIV.pkpool, p_ctx, worker_id());
    s->type = REQ_DONE;
    s->sync->count--;
  } else { 
    __sync_bool_compare_and_swap(&s->type, oldtype, REQ_PENDING);
  }
}

inline void proto_req_recv_long_init(MPIV_Request* req) {
  mv_key key = mv_make_key(req->rank, req->tag);
  mv_value value;
  RequestType oldtype = req->type;
  if (!hash_insert(MPIV.tbl, key, value)) {
    req->type = REQ_DONE;
    req->sync->count--;
  } else {
    __sync_bool_compare_and_swap(&req->type, oldtype, REQ_PENDING);
  }
}

inline void proto_req_send_long_init(MPIV_Request* req) {
  mv_key key = mv_make_key(req->rank, (1 << 30) | req->tag);
  mv_value value;
  RequestType oldtype = req->type;
  if (!hash_insert(MPIV.tbl, key, value)) {
    req->type = REQ_DONE;
    req->sync->count--;
  } else {
    __sync_bool_compare_and_swap(&req->type, oldtype, REQ_PENDING);
  }
}

inline bool init_sync(thread_sync* counter, int count, MPIV_Request* req) {
  bool ret = false;
  for (int i = 0; i < count; i++) {
    switch (req[i].type) {
      case REQ_NULL:
        break;
      case REQ_RECV_SHORT:
        req[i].sync = counter;
        proto_req_recv_short_init(&req[i]);
        ret = true;
        break;
      case REQ_RECV_LONG:
        req[i].sync = counter;
        proto_req_recv_long_init(&req[i]);
        ret = true;
        break;
      case REQ_SEND_LONG:
        req[i].sync = counter;
        proto_req_send_long_init(&req[i]);
        ret = true;
        break;
      case REQ_DONE:
        counter->count--;
        ret = true;
        break;
      case REQ_PENDING:
        ret = true;
        break;
      default:
        counter->count--;
        ret = true;
        break;
    }
  }
  return ret;
}

// Assume short message for now.
void mv_waitall(int count, MPIV_Request* req) {
  thread_sync* counter = thread_sync_get(count);
  if (init_sync(counter, count, req)) {
    while (counter->count > 0) {
      thread_wait(counter);
    }
    for (int i = 0; i < count; i++) {
      if (req[i].type == REQ_DONE) {
        req[i].type = REQ_NULL;
      } 
    }
  }
}

void mv_waitsome(int count, MPIV_Request* req, int* out_count, int* index) {
  *out_count = 0;
  thread_sync* counter = thread_sync_get(1);
  if (init_sync(counter, count, req)) {
    while (counter->count > 0) {
      thread_wait(counter);
      for (int i = 0; i < count; i++) {
        if (req[i].type == REQ_DONE) {
          index[*out_count] = i;
          *out_count = *out_count + 1;
          req[i].type = REQ_NULL;
          counter->count --;
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
