// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include "lci_internal.hpp"

namespace lci
{
void broadcast_x::call_impl(void* buffer, size_t size, int root,
                            runtime_t runtime, device_t device,
                            endpoint_t endpoint,
                            matching_engine_t matching_engine) const
{
  int seqnum = get_sequence_number();

  [[maybe_unused]] int round = 0;
  int rank = get_rank_me();
  int nranks = get_rank_n();

  if (nranks == 1) {
    return;
  }
  // binomial tree algorithm
  LCI_DBG_Log(LOG_TRACE, "collective",
              "enter broadcast %d (root %d buffer %p size %lu)\n", seqnum, root,
              buffer, size);
  bool has_data = (rank == root);
  int distance_left =
      (rank + nranks - root) % nranks;  // distance to the first rank on the
                                        // left that has the data (can be 0)
  int distance_right = (root - 1 - rank + nranks) %
                       nranks;  // number of empty ranks on the right
  int jump = std::ceil(nranks / 2.0);
  while (true) {
    if (has_data && jump <= distance_right) {
      // send to the right
      int rank_to_send = (rank + jump) % nranks;
      LCI_DBG_Log(LOG_TRACE, "collective", "broadcast %d round %d send to %d\n",
                  seqnum, round, rank_to_send);
      post_send_x(rank_to_send, buffer, size, seqnum, COMP_NULL)
          .runtime(runtime)
          .device(device)
          .endpoint(endpoint)
          .matching_engine(matching_engine)();
    } else if (distance_left == jump) {
      // receive from the right
      int rank_to_recv = (rank - jump + nranks) % nranks;
      LCI_DBG_Log(LOG_TRACE, "collective",
                  "broadcast %d round %d recv from %d\n", seqnum, round,
                  rank_to_recv);
      post_recv_x(rank_to_recv, buffer, size, seqnum, COMP_NULL)
          .runtime(runtime)
          .device(device)
          .endpoint(endpoint)
          .matching_engine(matching_engine)();
      has_data = true;
    }
    // The rank on your left (or yourself) sends the data to a rank right of it
    // by `jump` distance. update the distances accordingly
    if (distance_left >= jump) {
      distance_left -= jump;
    } else {
      // distance_left < jump
      distance_right = std::min(jump - distance_left - 1, distance_right);
    }
    LCI_DBG_Log(
        LOG_TRACE, "collective",
        "broadcast %d round %d jump %d distance_left %d distance_right %d\n",
        seqnum, round, jump, distance_left, distance_right);
    ++round;
    if (jump == 1) {
      break;
    } else {
      jump = std::ceil(jump / 2.0);
    }
  }
  LCI_DBG_Log(LOG_TRACE, "collective",
              "leave broadcast %d (root %d buffer %p size %lu)\n", seqnum, root,
              buffer, size);
}

}  // namespace lci