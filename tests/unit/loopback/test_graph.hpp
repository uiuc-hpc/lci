// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

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

TEST(GRAPH, singlethread1)
{
  lci::g_runtime_init();
  lci::comp_t graph = lci::alloc_graph();
  auto a = add_node(graph, 1);
  auto b = add_node(graph, 2);
  auto c = add_node(graph, 3);
  auto d = add_node(graph, 4);
  add_edge(graph, lci::GRAPH_START, a);
  add_edge(graph, lci::GRAPH_START, b);
  add_edge(graph, lci::GRAPH_START, c);
  add_edge(graph, lci::GRAPH_START, d);
  add_edge(graph, a, lci::GRAPH_END);
  add_edge(graph, b, lci::GRAPH_END);
  add_edge(graph, c, lci::GRAPH_END);
  add_edge(graph, d, lci::GRAPH_END);

  lci::graph_start(graph);
  while (lci::graph_test(graph).error.is_retry()) continue;
  lci::g_runtime_fina();
}

struct sleep_op_x {
  int m_sec;
  lci::comp_t m_comp;
  void* m_user_context;
  std::thread* m_thread;
  sleep_op_x(std::thread* t_, int sec_, lci::comp_t comp_)
      : m_thread(t_), m_sec(sec_), m_comp(comp_)
  {
  }
  void user_context(void* user_context_) { m_user_context = user_context_; }
  lci::status_t operator()()
  {
    fprintf(stderr, "this=%p sec=%d\n", this, m_sec);
    *m_thread = std::thread(
        [](int sec, lci::comp_t comp, void* user_context) {
          fprintf(stderr, "sleep %d s\n", sec);
          sleep(sec);
          fprintf(stderr, "woke up\n");
          lci::comp_signal(comp, lci::status_t(user_context));
        },
        m_sec, m_comp, m_user_context);
    return lci::errorcode_t::posted;
  }
};

TEST(GRAPH, multithread1)
{
  lci::g_runtime_init();
  lci::comp_t graph = lci::alloc_graph();

  std::thread t;
  sleep_op_x sleep_op(&t, 3, graph);

  auto a = lci::graph_add_node_op(graph, sleep_op);

  add_edge(graph, lci::GRAPH_START, a);
  add_edge(graph, a, lci::GRAPH_END);

  lci::graph_start(graph);
  while (lci::graph_test(graph).error.is_retry()) continue;

  t.join();

  lci::g_runtime_fina();
}

}  // namespace test_graph