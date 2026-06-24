// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include "lci_internal.hpp"

namespace lci
{
void alltoall_x::call_impl(const void* sendbuf, void* recvbuf, size_t size,
                           runtime_t runtime, device_t device,
                           endpoint_t endpoint,
                           matching_engine_t matching_engine,
                           comp_t comp, comp_semantic_t comp_semantic) const
{
  int seqnum = get_sequence_number();

  int rank = get_rank_me();
  int nranks = get_rank_n();
  LCI_DBG_Log(LOG_TRACE, "collective",
              "enter alltoall %d (sendbuf %p recvbuf %p size %lu)\n", seqnum,
              sendbuf, recvbuf, size);

  void* self_recvbuf = static_cast<char*>(recvbuf) + rank * size;
  void* self_sendbuf =
      static_cast<char*>(const_cast<void*>(sendbuf)) + rank * size;
  memcpy(self_recvbuf, self_sendbuf, size);

  if (nranks == 1) {
    if (!comp.is_empty()) {
      lci::comp_signal(comp, status_t(errorcode_t::done));
    }
    LCI_DBG_Log(LOG_TRACE, "collective",
                "leave alltoall %d (sendbuf %p recvbuf %p size %lu)\n",
                seqnum, sendbuf, recvbuf, size);
    return;
  }

  comp_t graph = alloc_graph_x().runtime(runtime).comp(comp)();

  for (int i = 0; i < nranks; ++i) {
    if (i == rank) {
      continue;
    }
    void* current_recvbuf =
        static_cast<void*>(static_cast<char*>(recvbuf) + i * size);
    void* current_sendbuf = static_cast<void*>(
        static_cast<char*>(const_cast<void*>(sendbuf)) + i * size);

    auto recv_node = graph_add_node_op(
        graph, post_recv_x(i, current_recvbuf, size, seqnum, graph)
                   .runtime(runtime)
                   .device(device)
                   .endpoint(endpoint)
                   .matching_engine(matching_engine)
                   .comp_semantic(comp_semantic)
                   .allow_retry(false));
    auto send_node = graph_add_node_op(
        graph, post_send_x(i, current_sendbuf, size, seqnum, graph)
                   .runtime(runtime)
                   .device(device)
                   .endpoint(endpoint)
                   .matching_engine(matching_engine)
                   .comp_semantic(comp_semantic)
                   .allow_retry(false));
    graph_add_edge(graph, GRAPH_START, recv_node);
    graph_add_edge(graph, GRAPH_START, send_node);
    graph_add_edge(graph, recv_node, GRAPH_END);
    graph_add_edge(graph, send_node, GRAPH_END);
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

  LCI_DBG_Log(LOG_TRACE, "collective",
              "leave alltoall %d (sendbuf %p recvbuf %p size %lu)\n", seqnum,
              sendbuf, recvbuf, size);
}

}  // namespace lci
