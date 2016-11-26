#include "mv-inl.h"
#include "mv.h"

MV_INLINE void proto_req_recv_short_init(mv_engine* mv, MPIV_Request* req)
{
  mv_key key = mv_make_key(req->rank, req->tag);
  mv_value value = (mv_value)req;
  RequestType oldtype = req->type;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    packet* p_ctx = (packet*)value;
    memcpy(req->buffer, p_ctx->content.buffer, req->size);
    mv_pp_free_to(mv->pkpool, p_ctx, mv_my_worker_id());
    req->type = REQ_DONE;
    thread_signal(req->sync);
  } else {
    __sync_bool_compare_and_swap(&req->type, oldtype, REQ_PENDING);
  }
}

MV_INLINE void proto_req_recv_long_init(mv_engine* mv, MPIV_Request* req)
{
  mv_key key = mv_make_key(req->rank, req->tag);
  mv_value value = 0;
  RequestType oldtype = req->type;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    req->type = REQ_DONE;
    thread_signal(req->sync);
  } else {
    __sync_bool_compare_and_swap(&req->type, oldtype, REQ_PENDING);
  }
}

MV_INLINE void proto_req_send_long_init(mv_engine* mv, MPIV_Request* req)
{
  mv_key key = mv_make_key(req->rank, (1 << 30) | req->tag);
  mv_value value = 0;
  RequestType oldtype = req->type;
  if (!mv_hash_insert(mv->tbl, key, &value)) {
    req->type = REQ_DONE;
    thread_signal(req->sync);
  } else {
    __sync_bool_compare_and_swap(&req->type, oldtype, REQ_PENDING);
  }
}

MV_INLINE bool init_sync(mv_engine* mv, mv_sync* counter, int count,
                         MPIV_Request* req)
{
  bool ret = false;
  for (int i = 0; i < count; i++) {
    switch (req[i].type) {
      case REQ_NULL:
        break;
      case REQ_RECV_SHORT:
        req[i].sync = counter;
        proto_req_recv_short_init(mv, &req[i]);
        ret = true;
        break;
      case REQ_RECV_LONG:
        req[i].sync = counter;
        proto_req_recv_long_init(mv, &req[i]);
        ret = true;
        break;
      case REQ_SEND_LONG:
        req[i].sync = counter;
        proto_req_send_long_init(mv, &req[i]);
        ret = true;
        break;
      case REQ_DONE:
        thread_signal(req->sync);
        ret = true;
        break;
      case REQ_PENDING:
        ret = true;
        break;
      default:
        thread_signal(req->sync);
        ret = true;
        break;
    }
  }
  return ret;
}

// Assume short message for now.
void mv_waitall(mv_engine* mv, int count, MPIV_Request* req)
{
  mv_sync* counter = mv_get_counter(count);
  if (init_sync(mv, counter, count, req)) {
    thread_wait(counter);
    for (int i = 0; i < count; i++) {
      if (req[i].type == REQ_DONE) {
        req[i].type = REQ_NULL;
      }
    }
  }
}

#if 0
void mv_waitsome(int count, MPIV_Request* req, int* out_count, int* index) {
  *out_count = 0;
  mv_sync* counter = mv_get_counter(1);
  if (init_sync(counter, count, req)) {
    while (*out_count == 0) {
      thread_wait(counter);
      for (int i = 0; i < count; i++) {
        if (req[i].type == REQ_DONE) {
          index[*out_count] = i;
          *out_count = *out_count + 1;
          req[i].type = REQ_NULL;
          thread_signal(counter);
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
