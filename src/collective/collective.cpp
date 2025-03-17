// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include "lci_internal.hpp"

namespace lci
{
void barrier_x::call_impl(runtime_t runtime, device_t device,
                          endpoint_t endpoint,
                          matching_engine_t matching_engine,
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
    post_recv_x(rank_to_recv, &dummy, sizeof(dummy), 0, comp)
        .runtime(runtime)
        .device(device)
        .endpoint(endpoint)
        .matching_engine(matching_engine)
        .allow_retry(false)
        .allow_ok(false)();
    auto post_send_op =
        post_send_x(rank_to_send, &dummy, sizeof(dummy), 0, comp)
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

}  // namespace lci