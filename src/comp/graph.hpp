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
        m_end_signals_expected(0),
        m_end_value(attr_.user_context)
  {
    attr.comp_type = attr_comp_type_t::graph;
  }
  ~graph_t()
  {
    for (auto node : nodes) {
      delete reinterpret_cast<node_impl_t*>(node);
    }
  }

  static void trigger_node(graph_node_t node);
  static void mark_complete(graph_node_t node, status_t status);

  graph_node_t add_node(graph_node_run_cb_t fn, void* value = nullptr,
                        graph_node_free_cb_t free_cb = nullptr);
  void add_edge(graph_node_t src, graph_node_t dst,
                graph_edge_run_cb_t fn = nullptr);
  void start();
  void signal(status_t status) override;
  status_t test();

 private:
  struct node_impl_t {
    std::atomic<int> signals_received;
    int signals_expected;
    graph_t* graph;
    graph_node_run_cb_t fn;
    void* value;
    graph_node_free_cb_t free_cb;
    std::vector<std::pair<graph_node_t, graph_edge_run_cb_t>> out_edges;
    node_impl_t(graph_t* graph_, graph_node_run_cb_t fn_, void* value_,
                graph_node_free_cb_t free_cb_)
        : signals_received(0),
          signals_expected(0),
          graph(graph_),
          fn(fn_),
          value(value_),
          free_cb(free_cb_)
    {
    }
    ~node_impl_t()
    {
      if (free_cb) {
        free_cb(value);
      }
    }
  };

  comp_t m_comp;
  // the graph structure
  std::vector<graph_node_t> m_start_nodes;
  std::vector<graph_node_t> nodes;
  int m_end_signals_expected;
  void* m_end_value;
  char padding[LCI_CACHE_LINE];
  // needed when executing the graph
  std::atomic<int> m_end_signals_received;
};

inline void graph_t::trigger_node(graph_node_t node_)
{
  auto node = reinterpret_cast<node_impl_t*>(node_);
  status_t status;
  if (node->fn) {
    status = node->fn(node->value);
  }
  LCI_DBG_Log(LOG_TRACE, "graph", "graph %p trigger node %p, status %s\n",
              node->graph, node_, status.error.get_str());
  if (status.is_done()) {
    mark_complete(node_, status);
  } else {
    LCI_Assert(!status.is_retry(),
               "The node function should not return retry. Try set "
               "allow_retry(false) in the operation.");
  }
}

inline void graph_t::mark_complete(graph_node_t node_, status_t status)
{
  auto node = reinterpret_cast<node_impl_t*>(node_);
  node->signals_received = 0;  // reset the signal
  auto src_value = node->value;
  auto graph = node->graph;
  LCI_DBG_Log(LOG_TRACE, "graph", "graph %p mark complete node %p, status %s\n",
              graph, node_, status.error.get_str());
  bool graph_completed = false;
  for (auto edge : node->out_edges) {
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
      int signals =
          next_node->signals_received.fetch_add(1, std::memory_order_relaxed) +
          1;
      if (signals == next_node->signals_expected) {
        trigger_node(next_node_);
      } else {
        LCI_Assert(signals < next_node->signals_expected,
                   "The number of signals (%d) should not exceed the expected "
                   "number (%d)",
                   signals, next_node->signals_expected);
      }
    } else {
      int signals = graph->m_end_signals_received.fetch_add(
                        1, std::memory_order_relaxed) +
                    1;
      if (signals == graph->m_end_signals_expected) {
        graph_completed = true;
      } else {
        LCI_Assert(signals < graph->m_end_signals_expected,
                   "The number of signals (%d) should not exceed the expected "
                   "number (%d)",
                   signals, graph->m_end_signals_expected);
      }
    }
  }
  if (graph_completed) {
    // the graph is completed
    LCI_DBG_Log(LOG_TRACE, "graph", "graph %p completed\n", graph);
    comp_t comp = graph->m_comp;
    if (!comp.is_empty()) {
      status_t status;
      status.set_done();
      status.user_context = graph->m_end_value;
      delete graph;
      // we should not access any graph members after this point
      comp.get_impl()->signal(status);
    }
  }
}

inline graph_node_t graph_t::add_node(graph_node_run_cb_t fn, void* value,
                                      graph_node_free_cb_t free_cb)
{
  auto ret = new node_impl_t(this, fn, value, free_cb);
  nodes.push_back(ret);
  return reinterpret_cast<graph_node_t>(ret);
}

inline void graph_t::add_edge(graph_node_t src_, graph_node_t dst_,
                              graph_edge_run_cb_t fn)
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
    ++m_end_signals_expected;
  } else {
    ++dst->signals_expected;
  }
}

inline void graph_t::start()
{
  LCI_DBG_Log(LOG_TRACE, "graph", "graph %p start\n", this);
  m_end_signals_received = 0;
  for (auto node_ : m_start_nodes) {
    auto node = reinterpret_cast<node_impl_t*>(node_);
    LCI_Assert(node->signals_expected == 0,
               "Start node should not have any incoming edges (signals: %d)",
               node->signals_expected);
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
  if (m_end_signals_received.load(std::memory_order_relaxed) ==
      m_end_signals_expected) {
    status.set_done();
    status.user_context = m_end_value;
    return status;
  } else {
    status.set_retry();
    return status;
  }
}

inline void graph_node_mark_complete_x::call_impl(graph_node_t node,
                                                  status_t status,
                                                  runtime_t) const
{
  graph_t::mark_complete(node, status);
}

inline graph_node_t graph_add_node_x::call_impl(comp_t comp,
                                                graph_node_run_cb_t fn,
                                                void* value,
                                                graph_node_free_cb_t free_cb,
                                                runtime_t) const
{
  auto graph = reinterpret_cast<graph_t*>(comp.get_impl());
  return graph->add_node(fn, value, free_cb);
}

inline void graph_add_edge_x::call_impl(comp_t comp, graph_node_t src,
                                        graph_node_t dst,
                                        graph_edge_run_cb_t fn, runtime_t) const
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