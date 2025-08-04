// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include "lci_internal.hpp"

namespace lci
{
void alltoall_x::call_impl(const void* sendbuf, void* recvbuf, size_t size,
                           runtime_t runtime, device_t device,
                           endpoint_t endpoint,
                           matching_engine_t matching_engine) const
{
  int seqnum = get_sequence_number();

  int rank = get_rank_me();
  int nranks = get_rank_n();
  LCI_DBG_Log(LOG_TRACE, "collective",
              "enter alltoall %d (sendbuf %p recvbuf %p size %lu)\n", seqnum,
              sendbuf, recvbuf, size);

  comp_t comp = alloc_sync_x().threshold(2 * nranks - 2).runtime(runtime)();

  for (int i = 0; i < nranks; ++i) {
    void* current_recvbuf =
        static_cast<void*>(static_cast<char*>(recvbuf) + i * size);
    void* current_sendbuf = static_cast<void*>(
        static_cast<char*>(const_cast<void*>(sendbuf)) + i * size);
    if (i == rank) {
      memcpy(current_recvbuf, current_sendbuf, size);
      continue;
    }
    status_t status;
    do {
      status = post_recv_x(i, current_recvbuf, size, seqnum, comp)
                   .runtime(runtime)
                   .device(device)
                   .endpoint(endpoint)
                   .matching_engine(matching_engine)
                   .allow_done(false)();
      progress_x().runtime(runtime).device(device).endpoint(endpoint)();
    } while (status.is_retry());
    do {
      status = post_send_x(i, current_sendbuf, size, seqnum, comp)
                   .runtime(runtime)
                   .device(device)
                   .endpoint(endpoint)
                   .matching_engine(matching_engine)
                   .allow_done(false)();
      progress_x().runtime(runtime).device(device).endpoint(endpoint)();
    } while (status.is_retry());
  }

  // sync_wait_x(comp, nullptr).runtime(runtime)();
  while (!sync_test_x(comp, nullptr).runtime(runtime)()) {
    progress_x().runtime(runtime).device(device).endpoint(endpoint)();
  }
  free_comp(&comp);
}

}  // namespace lci