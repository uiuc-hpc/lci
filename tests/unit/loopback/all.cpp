// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <random>
#include <iterator>
#include "lci.hpp"
#include "util.hpp"
// #include "test_basic.hpp"
// #include "test_network.hpp"
// #include "test_mpmc_array.hpp"
// #include "test_mpmc_set.hpp"
// #include "test_matching_engine.hpp"
// #include "test_sync.hpp"
// #include "test_cq.hpp"
// #include "test_graph.hpp"
// #include "test_am.hpp"
// #include "test_sendrecv.hpp"
// #include "test_put.hpp"
// #include "test_putImm.hpp"
// #include "test_get.hpp"
// #include "test_backlog_queue.hpp"
// #include "test_matching_policy.hpp"

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

namespace test_graph
{
lci::graph_node_t add_node(lci::comp_t graph, uint64_t value)
{
  return lci::graph_add_node_x(graph,
                               [](void* value) -> lci::status_t {
                                 fprintf(stderr, "execute node %p\n", value);
                                 return lci::errorcode_t::ok;
                               })
      .value(reinterpret_cast<void*>(value))();
}

void add_edge(lci::comp_t graph, lci::graph_node_t src, lci::graph_node_t dst)
{
  lci::graph_add_edge_x(graph, src, dst)
      .fn([](lci::status_t status, void* src_value, void* dst_value) {
        fprintf(stderr, "execute edge %p -> %p\n", src_value, dst_value);
      })();
}

TEST(GRAPH, singlethread0)
{
  lci::g_runtime_init();
  lci::comp_t graph = lci::alloc_graph();
  auto a = add_node(graph, 1);
  auto b = add_node(graph, 2);
  add_edge(graph, lci::GRAPH_START, a);
  add_edge(graph, a, b);
  add_edge(graph, b, lci::GRAPH_END);

  lci::graph_start(graph);
  while (lci::graph_test(graph).error.is_retry()) continue;
  lci::g_runtime_fina();
}

// TEST(GRAPH, singlethread1)
// {
//   lci::g_runtime_init();
//   lci::comp_t graph = lci::alloc_graph();
//   auto a = add_node(graph, 1);
//   auto b = add_node(graph, 2);
//   auto c = add_node(graph, 3);
//   auto d = add_node(graph, 4);
//   add_edge(graph, lci::GRAPH_START, a);
//   add_edge(graph, lci::GRAPH_START, b);
//   add_edge(graph, lci::GRAPH_START, c);
//   add_edge(graph, lci::GRAPH_START, d);
//   add_edge(graph, a, lci::GRAPH_END);
//   add_edge(graph, b, lci::GRAPH_END);
//   add_edge(graph, c, lci::GRAPH_END);
//   add_edge(graph, d, lci::GRAPH_END);

//   lci::graph_start(graph);
//   while (lci::graph_test(graph).error.is_retry()) continue;
//   lci::g_runtime_fina();
// }

struct sleep_op_x {
  int sec;
  lci::comp_t comp;
  void* user_context;
  std::thread t;
  sleep_op_x(int sec_, lci::comp_t comp_) : sec(sec_), comp(comp_) {}
  void set_user_context(void* user_context_) { user_context = user_context_; }
  lci::status_t operator()()
  {
    t = std::thread([this]() {
      fprintf(stderr, "start to sleep\n");
      sleep(sec);
      fprintf(stderr, "woke up\n");
      lci::comp_signal(comp, lci::status_t(user_context));
    });
    return lci::errorcode_t::posted;
  }
  void wait() { t.join(); }
};

TEST(GRAPH, multithread1)
{
  lci::g_runtime_init();
  lci::comp_t graph = lci::alloc_graph();

  sleep_op_x sleep_op(3, graph);
  std::function<lci::status_t()> f = std::ref(sleep_op);
  void* data = new std::function<lci::status_t()>(f);
  auto a = lci::graph_add_node_x(graph, lci::graph_execute_op_fn).value(data)();
  sleep_op.set_user_context(a);

  add_edge(graph, lci::GRAPH_START, a);
  add_edge(graph, a, lci::GRAPH_END);

  lci::graph_start(graph);
  while (lci::graph_test(graph).error.is_retry()) continue;

  sleep_op.wait();

  lci::g_runtime_fina();
}

}  // namespace test_graph