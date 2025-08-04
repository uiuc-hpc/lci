// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include "lci_internal.hpp"

namespace lci
{
void barrier_x::call_impl(runtime_t runtime, device_t device,
                          endpoint_t endpoint,
                          matching_engine_t matching_engine,
                          comp_semantic_t comp_semantic, comp_t comp) const
{
  int seqnum = get_sequence_number();
  [[maybe_unused]] int round = 0;
  int rank = get_rank_me();
  int nranks = get_rank_n();

  // dissemination algorithm
  LCI_DBG_Log(LOG_TRACE, "collective", "enter barrier %d\n", seqnum);
  if (nranks == 1) {
    if (!comp.is_empty()) {
      lci::comp_signal(comp, status_t(errorcode_t::done));
    }
    return;
  }
  comp_t graph = alloc_graph_x().runtime(runtime).comp(comp)();
  graph_node_t old_node = GRAPH_START;
  graph_node_t dummy_node;
  for (int jump = 1; jump < nranks; jump *= 2) {
    int rank_to_recv = (rank - jump + nranks) % nranks;
    int rank_to_send = (rank + jump) % nranks;
    LCI_DBG_Log(LOG_TRACE, "collective",
                "barrier %d round %d recv from %d send to %d\n", seqnum,
                round++, rank_to_recv, rank_to_send);

    auto recv = post_recv_x(rank_to_recv, nullptr, 0, seqnum, graph)
                    .runtime(runtime)
                    .device(device)
                    .endpoint(endpoint)
                    .matching_engine(matching_engine)
                    .allow_retry(false);
    auto send = post_send_x(rank_to_send, nullptr, 0, seqnum, graph)
                    .runtime(runtime)
                    .device(device)
                    .endpoint(endpoint)
                    .matching_engine(matching_engine)
                    .allow_retry(false);
    if (jump * 2 >= nranks) {
      // this is the last round, need to take care of the completion semantic
      send = send.comp_semantic(comp_semantic);
      dummy_node = GRAPH_END;
    } else {
      dummy_node = graph_add_node(
          graph, [](void*) -> status_t { return errorcode_t::done; });
    }
    auto recv_node = graph_add_node_op(graph, recv);
    auto send_node = graph_add_node_op(graph, send);
    graph_add_edge(graph, old_node, recv_node);
    graph_add_edge(graph, old_node, send_node);
    graph_add_edge(graph, recv_node, dummy_node);
    graph_add_edge(graph, send_node, dummy_node);
    old_node = dummy_node;
  }
  graph_start(graph);
  if (comp.is_empty()) {
    // blocking wait
    status_t status;
    do {
      progress_x().runtime(runtime).device(device).endpoint(endpoint)();
      status = graph_test(graph);
    } while (status.is_retry());
    free_comp(&graph);
  }
  LCI_DBG_Log(LOG_TRACE, "collective", "leave barrier %d\n", seqnum);
}

}  // namespace lci