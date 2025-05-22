// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include <unistd.h>
#include <cstdio>
#include "lci.hpp"

// This examples shows the usage of the completion graph and the send/recv
// operations.

// create the graph according to the dissemination algorithm
lci::comp_t create_ibarrier_graph()
{
  int rank_me = lci::get_rank_me();
  int rank_n = lci::get_rank_n();

  lci::comp_t graph = lci::alloc_graph();
  // GRAPH_START is a special node that indicates the start of the graph.
  lci::graph_node_t old_node = lci::GRAPH_START;
  lci::graph_node_t dummy_node;
  // The dissemination algorithm contains log2(rank_n) rounds.
  // In each round, each rank sends and receives a message to/from rank_me +/-
  // round**2.
  for (int jump = 1; jump < rank_n; jump *= 2) {
    int rank_to_recv = (rank_me - jump + rank_n) % rank_n;
    int rank_to_send = (rank_me + jump) % rank_n;
    // Define the communication operations for each round.
    // We cannot explicitly retry in the graph, so we set allow_retry to false.
    // The runtime will handle the retry using the internal backlog queues.
    auto recv =
        lci::post_recv_x(rank_to_recv, nullptr, 0, 0, graph).allow_retry(false);
    auto send =
        lci::post_send_x(rank_to_send, nullptr, 0, 0, graph).allow_retry(false);
    // Note that we do not trigger the operations here.
    // Instead, we make them as nodes in the graph.
    auto recv_node = lci::graph_add_node_op(graph, recv);
    auto send_node = lci::graph_add_node_op(graph, send);
    // To make the code more readable, we use a dummy node to represent the end
    // of the round.
    if (jump * 2 >= rank_n) {
      // this is the last round
      // GRAPH_END is a special node that indicates the end of the graph.
      dummy_node = lci::GRAPH_END;
    } else {
      // we can make arbitrary functions as nodes in the graph
      // The graph expect the function of the node to either return `done` or
      // `posted`. In the case of `done`, the runtime will immeidately trigger
      // its children. In the case of `posted`, the runtime will do nothing and
      // the node will be considered pending until
      // `graph_node_mark_complete(node)` is called.
      dummy_node = graph_add_node(
          graph, [](void*) -> lci::status_t { return lci::errorcode_t::done; });
    }
    // Specify the dependencies between the nodes.
    // Wait for the previous round to finish before starting this round.
    lci::graph_add_edge(graph, old_node, recv_node);
    lci::graph_add_edge(graph, old_node, send_node);
    // Wait for the send and recv operations to finish before moving to the next
    // round.
    lci::graph_add_edge(graph, recv_node, dummy_node);
    lci::graph_add_edge(graph, send_node, dummy_node);
    old_node = dummy_node;
  }
  return graph;
}

int main()
{
  lci::g_runtime_init();

  // create a graph describing the operations needed by the barrier
  lci::comp_t graph = create_ibarrier_graph();

  // the graph can be reused
  for (int i = 0; i < 3; ++i) {
    if (lci::get_rank_me() == 0) {
      // create some asymmetric delay
      sleep(1);
    }
    fprintf(stderr, "rank %d start barrier\n", lci::get_rank_me());
    // start executing those operations
    lci::graph_start(graph);
    // wait for the operations to finish
    while (lci::graph_test(graph).is_retry()) {
      lci::progress();
    }
    fprintf(stderr, "rank %d end barrier\n", lci::get_rank_me());
  }

  // free the graph
  lci::free_comp(&graph);

  lci::g_runtime_fina();
  return 0;
}