// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include "lci_internal.hpp"

namespace lci
{
namespace
{
void build_graph_direct(comp_t graph, post_send_x send_op, post_recv_x recv_op,
                        int root)
{
  int rank = get_rank_me();
  int nranks = get_rank_n();

  if (rank == root) {
    for (int i = 0; i < nranks; ++i) {
      int target = (i + rank) % nranks;
      if (target == rank) {
        // skip self
        continue;
      }
      graph_node_t send_node = graph_add_node_op(graph, send_op.rank(target));
      graph_add_edge(graph, GRAPH_START, send_node);
      graph_add_edge(graph, send_node, GRAPH_END);
    }
  } else {
    // non-root ranks only receive
    graph_node_t recv_node = graph_add_node_op(graph, recv_op.rank(root));
    graph_add_edge(graph, GRAPH_START, recv_node);
    graph_add_edge(graph, recv_node, GRAPH_END);
  }
}

void build_graph_tree(comp_t graph, post_send_x send_op, post_recv_x recv_op,
                      int root)
{
  [[maybe_unused]] int round = 0;

  int rank = get_rank_me();
  int nranks = get_rank_n();

  // binomial tree algorithm
  graph_node_t old_node = GRAPH_START;

  bool has_data = (rank == root);
  int distance_left =
      (rank + nranks - root) % nranks;  // distance to the first rank on the
                                        // left that has the data (can be 0)
  int distance_right = (root - 1 - rank + nranks) %
                       nranks;  // number of empty ranks on the right
  int jump = std::ceil(nranks / 2.0);
  while (true) {
    if (has_data && jump <= distance_right) {
      // send to the right
      int rank_to_send = (rank + jump) % nranks;
      LCI_DBG_Log(LOG_TRACE, "collective",
                  "broadcast (tree) round %d send to %d\n", round,
                  rank_to_send);
      auto send_node = graph_add_node_op(graph, send_op.rank(rank_to_send));
      graph_add_edge(graph, old_node, send_node);
      old_node = send_node;
    } else if (distance_left == jump) {
      // receive from the right
      int rank_to_recv = (rank - jump + nranks) % nranks;
      LCI_DBG_Log(LOG_TRACE, "collective",
                  "broadcast (tree) round %d recv from %d\n", round,
                  rank_to_recv);
      auto recv_node = graph_add_node_op(graph, recv_op.rank(rank_to_recv));
      graph_add_edge(graph, old_node, recv_node);
      old_node = recv_node;
      has_data = true;
    }
    // The rank on your left (or yourself) sends the data to a rank right of it
    // by `jump` distance. update the distances accordingly
    if (distance_left >= jump) {
      distance_left -= jump;
    } else {
      // distance_left < jump
      distance_right = std::min(jump - distance_left - 1, distance_right);
    }
    // LCI_DBG_Log(
    //     LOG_TRACE, "collective",
    //     "broadcast %d round %d jump %d distance_left %d distance_right %d\n",
    //     seqnum, round, jump, distance_left, distance_right);
    ++round;
    if (jump == 1) {
      break;
    } else {
      jump = std::ceil(jump / 2.0);
    }
  }
  graph_add_edge(graph, old_node, GRAPH_END);
}

void build_graph_ring(comp_t graph, post_send_x send_op, post_recv_x recv_op,
                      int root, void* buffer, size_t size, int nsteps)
{
  int rank = get_rank_me();
  int nranks = get_rank_n();

  size_t step_size = (size + nsteps - 1) / nsteps;
  int left = (rank - 1 + nranks) % nranks;
  int right = (rank + 1) % nranks;
  send_op = send_op.rank(right);
  recv_op = recv_op.rank(left);

  graph_node_t old_node = GRAPH_START;
  for (int step = 0; step <= nsteps; ++step) {
    graph_node_t next_node;

    if ((step == 0 && rank == root) || (step == nsteps && right == root)) {
      // they won't do anything in this step
      if (step == nsteps) {
        graph_add_edge(graph, old_node, GRAPH_END);
      }
      continue;
    }

    if (step == nsteps)
      next_node = GRAPH_END;
    else
      next_node = graph_add_node(graph, GRAPH_NODE_DUMMY_CB);

    if (step != nsteps && rank != root) {
      // receive from the previous rank
      int idx = step;
      void* step_buffer = static_cast<char*>(buffer) + idx * step_size;
      size_t actual_size = std::min(step_size, size - idx * step_size);
      graph_node_t recv_node = graph_add_node_op(
          graph, recv_op.local_buffer(step_buffer).size(actual_size));
      graph_add_edge(graph, old_node, recv_node);
      graph_add_edge(graph, recv_node, next_node);
      LCI_Log(LOG_TRACE, "collective",
              "broadcast (ring) step %d recv from %d size %lu\n", step, left,
              actual_size);
    }

    if (step != 0 && right != root) {
      // send to the next rank
      int idx = step - 1;
      void* step_buffer = static_cast<char*>(buffer) + idx * step_size;
      size_t actual_size = std::min(step_size, size - idx * step_size);
      graph_node_t send_node = graph_add_node_op(
          graph, send_op.local_buffer(step_buffer).size(actual_size));
      graph_add_edge(graph, old_node, send_node);
      graph_add_edge(graph, send_node, next_node);
      LCI_Log(LOG_TRACE, "collective",
              "broadcast (ring) step %d send to %d size %lu\n", step, left,
              actual_size);
    }

    old_node = next_node;
  }
}
}  // namespace

void broadcast_x::call_impl(void* buffer, size_t size, int root,
                            runtime_t runtime, device_t device,
                            endpoint_t endpoint,
                            matching_engine_t matching_engine, comp_t comp,
                            broadcast_algorithm_t algorithm,
                            int ring_nsteps) const
{
  int seqnum = get_sequence_number();
  int nranks = get_rank_n();

  LCI_DBG_Log(LOG_TRACE, "collective",
              "enter broadcast %d (root %d buffer %p size %lu)\n", seqnum, root,
              buffer, size);
  if (nranks == 1) {
    if (!comp.is_empty()) {
      lci::comp_signal(comp, status_t(errorcode_t::done));
    }
    return;
  }

  comp_t graph = alloc_graph_x().runtime(runtime).comp(comp)();
  auto send_op = post_send_x(-1, buffer, size, seqnum, graph)
                     .runtime(runtime)
                     .device(device)
                     .endpoint(endpoint)
                     .matching_engine(matching_engine)
                     .allow_retry(false);
  auto recv_op = post_recv_x(-1, buffer, size, seqnum, graph)
                     .runtime(runtime)
                     .device(device)
                     .endpoint(endpoint)
                     .matching_engine(matching_engine)
                     .allow_retry(false);

  if (algorithm == broadcast_algorithm_t::none) {
    // auto select the best algorithm
    if (size <= 65536 /* FIXME: magic number */) {
      if (nranks <= 8) {
        algorithm = broadcast_algorithm_t::direct;
      } else {
        algorithm = broadcast_algorithm_t::tree;
      }
    } else {
      algorithm = broadcast_algorithm_t::ring;
    }
  }

  if (algorithm == broadcast_algorithm_t::direct) {
    // direct algorithm
    build_graph_direct(graph, send_op, recv_op, root);
  } else if (algorithm == broadcast_algorithm_t::tree) {
    // binomial tree algorithm
    build_graph_tree(graph, send_op, recv_op, root);
  } else if (algorithm == broadcast_algorithm_t::ring) {
    build_graph_ring(graph, send_op, recv_op, root, buffer, size, ring_nsteps);
  } else {
    LCI_Assert(false, "Unsupported broadcast algorithm %d",
               static_cast<int>(algorithm));
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
              "leave broadcast %d (root %d buffer %p size %lu)\n", seqnum, root,
              buffer, size);
}

}  // namespace lci