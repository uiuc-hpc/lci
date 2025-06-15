// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include "lci_internal.hpp"

namespace lci
{
namespace
{
struct reduce_wrapper_args_t {
  void* tmp_buffer;
  void* recvbuf;
  size_t count;
  reduce_op_t op;
};

status_t reduce_wrapper_fn(void* args)
{
  auto* wrapper_args = static_cast<reduce_wrapper_args_t*>(args);
  wrapper_args->op(wrapper_args->tmp_buffer, wrapper_args->recvbuf,
                   wrapper_args->recvbuf, wrapper_args->count);
  delete wrapper_args;
  return errorcode_t::done;
}

void build_graph_direct(comp_t graph, post_send_x send_op, post_recv_x recv_op,
                        const void* sendbuf, void* recvbuf, size_t recvcount,
                        size_t item_size, reduce_op_t op)
{
  int rank = get_rank_me();
  int nranks = get_rank_n();

  char* sendbuf_c = const_cast<char*>(static_cast<const char*>(sendbuf));
  char* recvbuf_c = static_cast<char*>(recvbuf);

  bool in_place = (recvbuf_c == sendbuf_c + rank * recvcount * item_size);
  void* tmp_buffer = malloc(recvcount * item_size * (nranks - 1));
  void* p = tmp_buffer;
  if (!in_place) {
    // copy the send buffer to the receive buffer
    memcpy(recvbuf, sendbuf_c + rank * recvcount * item_size,
           recvcount * item_size);
  }

  graph_node_t prev_reduce_node = nullptr;
  graph_node_t free_node =
      graph_add_node_x(graph, [](void* tmp_buffer) -> status_t {
        free(tmp_buffer);
        return errorcode_t::done;
      }).value(tmp_buffer)();
  for (int i = 1; i < nranks; ++i) {
    int target = (i + rank) % nranks;
    graph_node_t send_node = graph_add_node_op(
        graph, send_op.rank(target).local_buffer(
                   sendbuf_c + target * recvcount * item_size));
    graph_add_edge(graph, GRAPH_START, send_node);
    graph_add_edge(graph, send_node, GRAPH_END);
    graph_node_t recv_node =
        graph_add_node_op(graph, recv_op.rank(target).local_buffer(p));
    graph_node_t reduce_node =
        graph_add_node_x(graph, reduce_wrapper_fn)
            .value(new reduce_wrapper_args_t{p, recvbuf, recvcount, op})();
    graph_add_edge(graph, GRAPH_START, recv_node);
    graph_add_edge(graph, recv_node, reduce_node);
    if (prev_reduce_node) {
      graph_add_edge(graph, prev_reduce_node, reduce_node);
    }
    prev_reduce_node = reduce_node;
    p = static_cast<char*>(p) + recvcount * item_size;
  }
  graph_add_edge(graph, prev_reduce_node, free_node);
  graph_add_edge(graph, free_node, GRAPH_END);
}
}  // namespace

void reduce_scatter_x::call_impl(const void* sendbuf, void* recvbuf,
                                 size_t recvcount, size_t item_size,
                                 reduce_op_t op, runtime_t runtime,
                                 device_t device, endpoint_t endpoint,
                                 matching_engine_t matching_engine, comp_t comp,
                                 reduce_scatter_algorithm_t algorithm,
                                 [[maybe_unused]] int ring_nsteps) const
{
  int seqnum = get_sequence_number();
  int nranks = get_rank_n();

  LCI_DBG_Log(LOG_TRACE, "collective",
              "enter reduce_scatter %d (sendbuf %p recvbuf %p item_size %lu "
              "recvcount %lu)\n",
              seqnum, sendbuf, recvbuf, item_size, recvcount);
  if (nranks == 1) {
    if (recvbuf != sendbuf) {
      memcpy(recvbuf, sendbuf, item_size * recvcount);
    }
    if (!comp.is_empty()) {
      lci::comp_signal(comp, status_t(errorcode_t::done));
    }
    return;
  }

  comp_t graph = alloc_graph_x().runtime(runtime).comp(comp)();
  auto send_op = post_send_x(-1, const_cast<void*>(sendbuf),
                             recvcount * item_size, seqnum, graph)
                     .runtime(runtime)
                     .device(device)
                     .endpoint(endpoint)
                     .matching_engine(matching_engine)
                     .allow_retry(false);
  auto recv_op = post_recv_x(-1, recvbuf, recvcount * item_size, seqnum, graph)
                     .runtime(runtime)
                     .device(device)
                     .endpoint(endpoint)
                     .matching_engine(matching_engine)
                     .allow_retry(false);

  if (algorithm == reduce_scatter_algorithm_t::none) {
    // auto select the best algorithm
    algorithm = reduce_scatter_algorithm_t::direct;
  }

  if (algorithm == reduce_scatter_algorithm_t::direct) {
    // direct algorithm
    build_graph_direct(graph, send_op, recv_op, sendbuf, recvbuf, recvcount,
                       item_size, op);
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
              "leave reduce_scatter %d (sendbuf %p recvbuf %p item_size %lu "
              "count %lu)\n",
              seqnum, sendbuf, recvbuf, item_size, recvcount);
}

}  // namespace lci