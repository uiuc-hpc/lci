// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include "lci_internal.hpp"

namespace lci
{
void barrier_x::call_impl(runtime_t runtime, device_t device,
                          endpoint_t endpoint,
                          matching_engine_t matching_engine, tag_t tag,
                          comp_semantic_t comp_semantic) const
{
  static int count = -1;  // for debugging purpose
  ++count;
  int round = 0;
  // dissemination algorithm
  LCI_DBG_Log(LOG_TRACE, "collective", "enter barrier %d\n", count);
  uint8_t dummy = 0;
  int rank = get_rank();
  int nranks = get_nranks();
  for (int jump = 1; jump < nranks; jump *= 2) {
    int rank_to_recv = (rank - jump + nranks) % nranks;
    int rank_to_send = (rank + jump) % nranks;
    LCI_DBG_Log(LOG_TRACE, "collective",
                "barrier %d round %d recv from %d send to %d\n", count, round++,
                rank_to_recv, rank_to_send);
    comp_t comp = alloc_sync_x().threshold(2).runtime(runtime)();
    post_recv_x(rank_to_recv, &dummy, sizeof(dummy), tag, comp)
        .runtime(runtime)
        .device(device)
        .endpoint(endpoint)
        .matching_engine(matching_engine)
        .allow_retry(false)
        .allow_ok(false)();
    auto post_send_op =
        post_send_x(rank_to_send, &dummy, sizeof(dummy), tag, comp)
            .runtime(runtime)
            .device(device)
            .endpoint(endpoint)
            .matching_engine(matching_engine)
            .allow_retry(false)
            .allow_ok(false);
    if (jump * 2 >= nranks) {
      // this is the last round, need to take care of the completion semantic
      post_send_op = post_send_op.comp_semantic(comp_semantic);
    }
    post_send_op();
    while (!sync_test(comp, nullptr)) {
      progress_x().runtime(runtime).device(device).endpoint(endpoint)();
    }
    free_comp(&comp);
  }
  LCI_DBG_Log(LOG_TRACE, "collective", "leave barrier %d\n", count);
}

void broadcast_x::call_impl(int root, void* buffer, size_t size,
                            runtime_t runtime, device_t device,
                            endpoint_t endpoint,
                            matching_engine_t matching_engine, tag_t tag) const
{
  static int count = -1;  // for debugging purpose
  ++count;
  int round = 0;
  // binomial tree algorithm
  LCI_DBG_Log(LOG_TRACE, "collective",
              "enter broadcast %d (root %d buffer %p size %lu)\n", count, root,
              buffer, size);
  int rank = get_rank();
  int nranks = get_nranks();
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
                  count, round, rank_to_send);
      post_send_x(rank_to_send, buffer, size, tag, COMP_NULL_EXPECT_OK)
          .runtime(runtime)
          .device(device)
          .endpoint(endpoint)
          .matching_engine(matching_engine)();
    } else if (distance_left == jump) {
      // receive from the right
      int rank_to_recv = (rank - jump + nranks) % nranks;
      LCI_DBG_Log(LOG_TRACE, "collective",
                  "broadcast %d round %d recv from %d\n", count, round,
                  rank_to_recv);
      post_recv_x(rank_to_recv, buffer, size, tag, COMP_NULL_EXPECT_OK)
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
        count, round, jump, distance_left, distance_right);
    ++round;
    if (jump == 1) {
      break;
    } else {
      jump = std::ceil(jump / 2.0);
    }
  }
  LCI_DBG_Log(LOG_TRACE, "collective",
              "leave broadcast %d (root %d buffer %p size %lu)\n", count, root,
              buffer, size);
}

}  // namespace lci