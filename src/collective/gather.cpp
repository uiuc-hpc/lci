// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include "lci_internal.hpp"

namespace lci
{
void allgather_x::call_impl(const void* sendbuf, void* recvbuf, size_t size,
                            runtime_t runtime, device_t device,
                            endpoint_t endpoint,
                            matching_engine_t matching_engine) const
{
  int seqnum = get_sequence_number();

  int rank = get_rank_me();
  int nranks = get_rank_n();

  if (nranks == 1) {
    memcpy(recvbuf, sendbuf, size);
    return;
  }
  // alltoall algorithm
  comp_t sync = alloc_sync_x().threshold(2 * nranks - 2).runtime(runtime)();
  LCI_DBG_Log(LOG_TRACE, "collective",
              "enter allgather %d (sendbuf %p recvbuf %p size %lu)\n", seqnum,
              sendbuf, recvbuf, size);
  status_t status;
  for (int i = 1; i < nranks; ++i) {
    int peer_rank = (rank + i) % nranks;
    do {
      status =
          post_recv_x(peer_rank, static_cast<char*>(recvbuf) + peer_rank * size,
                      size, seqnum, sync)
              .runtime(runtime)
              .device(device)
              .endpoint(endpoint)
              .matching_engine(matching_engine)
              .allow_done(false)();
      progress_x().runtime(runtime).device(device).endpoint(endpoint)();
    } while (status.is_retry());
  }
  for (int i = 1; i < nranks; ++i) {
    int peer_rank = (rank + i) % nranks;
    do {
      status =
          post_send_x(peer_rank, const_cast<void*>(sendbuf), size, seqnum, sync)
              .runtime(runtime)
              .device(device)
              .endpoint(endpoint)
              .matching_engine(matching_engine)
              .allow_done(false)();
      progress_x().runtime(runtime).device(device).endpoint(endpoint)();
    } while (status.is_retry());
  }
  memcpy(static_cast<char*>(recvbuf) + rank * size, sendbuf, size);
  while (!sync_test(sync, nullptr)) {
    progress_x().runtime(runtime).device(device).endpoint(endpoint)();
  }
  free_comp(&sync);
  LCI_DBG_Log(LOG_TRACE, "collective",
              "leave allgather %d (sendbuf %p recvbuf %p size %lu)\n", seqnum,
              sendbuf, recvbuf, size);
}

}  // namespace lci