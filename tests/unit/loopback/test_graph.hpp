// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

namespace test_graph
{
TEST(GRAPH, singlethread)
{
  lci::g_runtime_init();
  lci::comp_t comp = lci::alloc_graph();
  lci::graph_node_t a =
      lci::graph_add_node_x(comp, [](void* value) -> lci::status_t {
        fprintf(stderr, "execute %p", value);
        return lci::errorcode_t::ok;
      }).value(reinterpret_cast<void*>(0x1))();
  lci::graph_node_t b =
      lci::graph_add_node_x(comp, [](void* value) -> lci::status_t {
        fprintf(stderr, "execute %p", value);
        return lci::errorcode_t::ok;
      }).value(reinterpret_cast<void*>(0x2))();
  lci::graph_add_edge_x(comp, a, b)
      .fn([](lci::status_t status, void* src_value, void* dst_value) {
        fprintf(stderr, "execute edge %p -> %p", src_value, dst_value);
      })();
  lci::graph_start(comp);
  while (lci::graph_test(comp).error.is_retry()) continue;
  lci::g_runtime_fina();
}

}  // namespace test_graph