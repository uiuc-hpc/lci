#ifndef RECV_H_
#define RECV_H_

void MPIV_Send(void* buffer, size_t size, int rank, int tag);

void MPIV_Recv(void* buffer, size_t size, int rank, int tag) {
  mpiv_value value;
  MPIV_Request s(buffer, size, rank, tag);

  mpiv_key key = mpiv_make_key(rank, tag);
  value.request = &s;

  if ((size_t)size > SHORT) {
    // RDNZ protocol, use SEND + WAIT.
    startt(misc_timing);
    char data[64];
    mpiv_packet* p = MPIV.pk_mgr.get_packet(data, RECV_READY, MPIV.me, tag);
    p->rdz = {0, (uintptr_t)&s, (uintptr_t)buffer, MPIV.ctx.heap_rkey,
              (uint32_t)size};
    MPIV_Send(data, 64, rank, 1 << 31 | tag);
    MPIV_Wait(s);
    stopt(misc_timing);
    return;
  }

  // Find if the message has arrived, if not go and make a request.
  startt(tbl_timing) auto p = MPIV.tbl.insert(key, value);
  auto in_val = p.first;
  stopt(tbl_timing)

      if (value.v == in_val.v) {
    MPIV_Wait(s);
    stopt(wake_timing)
  }
  else {
    mpiv_packet* p_ctx = in_val.packet;
    // This is a SHORT.
    // Parse the buffer.
    // Message has arrived, go and copy the message.
    startt(memcpy_timing) memcpy(buffer, p_ctx->egr.buffer, size);
    stopt(memcpy_timing);
    // Push here because the commthread must have posted a different one.
    startt(post_timing);
    MPIV.pk_mgr.new_packet(p_ctx);
    stopt(post_timing);
  }

  startt(tbl_timing) MPIV.tbl.erase(key, p.second);
  stopt(tbl_timing)
}

#endif
