// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#ifndef LCI_COMP_GRAPH_HPP
#define LCI_COMP_GRAPH_HPP

namespace lci
{
class graph_t : public comp_impl_t
{
 private:
  struct node_impl_t;

 public:
  graph_t(comp_attr_t attr_, comp_t comp_)
      : comp_impl_t(attr_),
        m_comp(comp_),
        m_end_signals_initial(0),
        m_end_value(nullptr)
  {
    attr.comp_type = attr_comp_type_t::graph;
  }
  ~graph_t() = default;

  static void trigger_node(graph_node_t node);
  static void mark_complete(graph_node_t node, status_t status);

  graph_node_t add_node(graph_node_fn_t fn, void* value = nullptr);
  void add_edge(graph_node_t src, graph_node_t dst,
                graph_edge_fn_t fn = nullptr);
  void start();
  void signal(status_t status) override;
  status_t test();

 private:
  struct node_impl_t {
    std::atomic<int> signals;
    graph_t* graph;
    graph_node_fn_t fn;
    void* value;
    std::vector<std::pair<graph_node_t, graph_edge_fn_t>> out_edges;
    node_impl_t(graph_t* graph_, graph_node_fn_t fn_, void* value_)
        : signals(0), graph(graph_), fn(fn_), value(value_)
    {
    }
  };

  comp_t m_comp;
  // the graph structure
  std::vector<graph_node_t> m_start_nodes;
  std::atomic<int> m_end_signals_initial;
  char padding[64];
  // needed when executing the graph
  std::atomic<int> m_end_signals_remain;
  void* m_end_value;
};

inline void graph_t::trigger_node(graph_node_t node_)
{
  auto node = reinterpret_cast<node_impl_t*>(node_);
  status_t status;
  if (node->fn) {
    status = node->fn(node->value);
  }
  if (status.error.is_ok()) {
    mark_complete(node_, status);
  } else {
    LCI_Assert(!status.error.is_retry(),
               "The node function should not return retry");
  }
}

inline void graph_t::mark_complete(graph_node_t node_, status_t status)
{
  auto node = reinterpret_cast<node_impl_t*>(node_);
  auto graph = node->graph;
  auto src_value = node->value;
  auto out_edges = node->out_edges;
  delete node;
  for (auto edge : out_edges) {
    auto next_node_ = edge.first;
    auto next_node = reinterpret_cast<node_impl_t*>(next_node_);
    auto edge_fn = edge.second;
    void* dst_value;
    // process the edge
    if (next_node_ == GRAPH_END) {
      dst_value = graph->m_end_value;
    } else {
      dst_value = next_node->value;
    }
    if (edge_fn) {
      edge_fn(status, src_value, dst_value);
    }

    // process the dst node
    if (next_node_ != GRAPH_END) {
      if (next_node->signals.fetch_sub(1, std::memory_order_relaxed) == 1) {
        trigger_node(next_node_);
      }
    } else {
      if (graph->m_end_signals_remain.fetch_sub(1, std::memory_order_relaxed) ==
          1) {
        // the graph is completed
        comp_t comp = graph->m_comp;
        if (!comp.is_empty()) {
          status_t status;
          status.error = errorcode_t::ok;
          status.user_context = graph->m_end_value;
          delete graph;
          comp.get_impl()->signal(status);
        }
      }
    }
  }
}

inline graph_node_t graph_t::add_node(graph_node_fn_t fn, void* value)
{
  auto ret = new node_impl_t(this, fn, value);
  return reinterpret_cast<graph_node_t>(ret);
}

inline void graph_t::add_edge(graph_node_t src_, graph_node_t dst_,
                              graph_edge_fn_t fn)
{
  LCI_Assert(dst_ != GRAPH_START,
             "The destination node should not be the start node");
  LCI_Assert(src_ != GRAPH_END, "The source node should not be the end node");
  if (src_ == GRAPH_START) {
    m_start_nodes.push_back(dst_);
    return;
  }
  auto src = reinterpret_cast<node_impl_t*>(src_);
  auto dst = reinterpret_cast<node_impl_t*>(dst_);
  src->out_edges.push_back({dst_, fn});
  if (dst_ == GRAPH_END) {
    m_end_signals_initial.fetch_add(1, std::memory_order_relaxed);
  } else {
    dst->signals.fetch_add(1, std::memory_order_relaxed);
  }
}

inline void graph_t::start()
{
  m_end_value = nullptr;
  m_end_signals_remain = m_end_signals_initial.load(std::memory_order_seq_cst);
  for (auto node_ : m_start_nodes) {
    auto node = reinterpret_cast<node_impl_t*>(node_);
    LCI_Assert(node->signals.load() == 0,
               "Start node should not have any incoming edges");
    trigger_node(node_);
  }
}

inline void graph_t::signal(status_t status)
{
  LCI_PCOUNTER_ADD(comp_produce, 1);
  LCI_PCOUNTER_ADD(comp_consume, 1);
  mark_complete(reinterpret_cast<graph_node_t>(status.user_context), status);
}

inline status_t graph_t::test()
{
  status_t status;
  if (m_end_signals_remain.load(std::memory_order_relaxed) == 0) {
    status.error = errorcode_t::ok;
    status.user_context = m_end_value;
    return status;
  } else {
    status.error = errorcode_t::retry;
    return status;
  }
}

inline void graph_node_mark_complete_x::call_impl(graph_node_t node,
                                                  status_t status,
                                                  runtime_t) const
{
  graph_t::mark_complete(node, status);
}

inline graph_node_t graph_add_node_x::call_impl(comp_t comp, graph_node_fn_t fn,
                                                void* value, runtime_t) const
{
  auto graph = reinterpret_cast<graph_t*>(comp.get_impl());
  return graph->add_node(fn, value);
}

inline void graph_add_edge_x::call_impl(comp_t comp, graph_node_t src,
                                        graph_node_t dst, graph_edge_fn_t fn,
                                        runtime_t) const
{
  auto graph = reinterpret_cast<graph_t*>(comp.get_impl());
  graph->add_edge(src, dst, fn);
}

inline void graph_start_x::call_impl(comp_t comp, runtime_t) const
{
  auto graph = reinterpret_cast<graph_t*>(comp.get_impl());
  graph->start();
}

inline status_t graph_test_x::call_impl(comp_t comp, runtime_t) const
{
  auto graph = reinterpret_cast<graph_t*>(comp.get_impl());
  return graph->test();
}

}  // namespace lci

#endif  // LCI_COMP_GRAPH_HPP