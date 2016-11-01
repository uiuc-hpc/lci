#ifndef WAITALL_H_
#define WAITALL_H_

#include <assert.h>
#include <mpi.h>

extern int mpiv_send_start, mpiv_send_end;
void mpiv_complete_rndz(Packet* p, MPIV_Request* s);
void proto_send_rdz(MPIV_Request* s);
void proto_send_short(const void* buffer, int size, int rank, int tag);

int proto_recv_short_wait(MPIV_Request* s) {
  mpiv_key key = mpiv_make_key(s->rank, s->tag);
  mpiv_value value;
  value.request = s;
  if (!MPIV.tbl.insert(key, value)) {
    Packet* p_ctx = value.packet;
    memcpy(s->buffer, p_ctx->buffer(), s->size);
    MPIV.pkpool.ret_packet_to(p_ctx, worker_id());
    s->done_ = true;
    return (s->counter->fetch_sub(1));
  }
  return 0;
}

// Assume short message for now.
void waitall(int count, MPIV_Request* req) {
  std::atomic<int> counter(count);
  for (int i = 0; i < count; i++) {
    req[i].counter = &counter;
  }
  for (int i = 0; i < count; i++) {
    proto_recv_short_wait(&req[i]);
  }

  // Loop in-case was signal wrongly.
  while (counter.load() != 0) MPIV_Wait(req);

  assert(counter.load() == 0);
}

#endif
