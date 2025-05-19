// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include "lci.hpp"

int main(int argc, char** argv)
{
  lci::g_runtime_init();
  lci::comp_t graph = lci::alloc_graph();

  auto send_op = lci::post_send_x(0, nullptr, 0, 0, graph).allow_retry(false);
  auto recv_op = lci::post_recv_x(0, nullptr, 0, 0, graph).allow_retry(false);
  auto send_node = lci::graph_add_node_op(graph, send_op);
  auto recv_node = lci::graph_add_node_op(graph, recv_op);

  auto a = lci::graph_add_node(graph, [](void*) -> lci::status_t {
    fprintf(stderr, "execute node a\n");
    return lci::errorcode_t::done;
  });
  auto b = lci::graph_add_node(graph, [](void*) -> lci::status_t {
    fprintf(stderr, "execute node b\n");
    return lci::errorcode_t::done;
  });

  lci::graph_add_edge(graph, lci::GRAPH_START, a);
  lci::graph_add_edge(graph, a, send_node);
  lci::graph_add_edge(graph, a, recv_node);
  lci::graph_add_edge(graph, send_node, b);
  lci::graph_add_edge(graph, recv_node, b);
  lci::graph_add_edge(graph, b, lci::GRAPH_END);

  for (int i = 0; i < 3; ++i) {
    lci::graph_start(graph);
    while (lci::graph_test(graph).error.is_retry()) lci::progress();
  }

  lci::free_comp(&graph);
  lci::g_runtime_fina();
  return 0;
}