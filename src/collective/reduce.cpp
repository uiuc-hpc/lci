// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include "lci_internal.hpp"

namespace lci
{
void reduce_x::call_impl(const void* sendbuf, void* recvbuf, size_t count,
                         size_t item_size, reduce_op_t op, int root,
                         runtime_t runtime, device_t device,
                         endpoint_t endpoint,
                         matching_engine_t matching_engine) const
{
  int seqnum = get_sequence_number();

  int round = 0;
  int rank = get_rank_me();
  int nranks = get_rank_n();

  if (nranks == 1) {
    if (recvbuf != sendbuf) {
      memcpy(recvbuf, sendbuf, item_size * count);
    }
    return;
  }
  // binomial tree algorithm
  LCI_DBG_Log(LOG_TRACE, "collective",
              "enter reduce %d (sendbuf %p recvbuf %p item_size %lu count %lu "
              "root %d)\n",
              seqnum, sendbuf, recvbuf, item_size, count, root);
  std::vector<std::pair<int, bool>> actions_per_round(
      std::ceil(std::log2(nranks)), {-1, false});
  // First compute the binary tree from the root to the leaves
  int nchildren = 0;
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
      actions_per_round[round] = {rank_to_send, true};
      ++nchildren;  // if there is a send, then there is one more child
    } else if (distance_left == jump) {
      // receive from the right
      int rank_to_recv = (rank - jump + nranks) % nranks;
      actions_per_round[round] = {rank_to_recv, false};
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
    ++round;
    if (jump == 1) {
      break;
    } else {
      jump = std::ceil(jump / 2.0);
    }
  }
  // Then replay the binary tree from the leaves to the root
  // also reverse the message direction
  bool to_free_tmp_buffer = false;
  bool to_free_data_buffer = false;
  void* tmp_buffer;   // to receive data
  void* data_buffer;  // to hold intermediate result
  if (rank == root) {
    // for the root, we can always use the recvbuf to hold the intermediate
    // result
    data_buffer = recvbuf;
    if (nchildren == 1 && sendbuf != recvbuf) {
      // if there is only one child, we can (almost always) use the data buffer
      // to receive, except for the case sendbuf = recvbuf = data_buffer
      tmp_buffer = data_buffer;
    } else {
      tmp_buffer = malloc(item_size * count);
      to_free_tmp_buffer = true;
    }
  } else {
    // for non-root
    if (nchildren == 1) {
      // if there is only one child, tmp_buffer is the data_buffer
      tmp_buffer = malloc(item_size * count);
      data_buffer = tmp_buffer;
      to_free_tmp_buffer = true;
    } else {
      tmp_buffer = malloc(item_size * count);
      to_free_tmp_buffer = true;
      data_buffer = malloc(item_size * count);
      to_free_data_buffer = true;
    }
  }
  bool has_received = false;
  for (int i = round - 1; i >= 0; --i) {
    int target_rank = actions_per_round[i].first;
    if (target_rank < 0) {
      continue;
    }
    bool is_send = !actions_per_round[i].second;  // reverse the direction
    if (is_send) {
      LCI_DBG_Log(LOG_TRACE, "collective", "reduce %d round %d send to %d\n",
                  seqnum, i, target_rank);
      void* buffer_to_send = const_cast<void*>(sendbuf);
      if (has_received) {
        buffer_to_send = data_buffer;
      }
      post_send_x(target_rank, buffer_to_send, item_size * count, seqnum,
                  COMP_NULL)
          .runtime(runtime)
          .device(device)
          .endpoint(endpoint)
          .matching_engine(matching_engine)();
      break;
    } else {
      LCI_DBG_Log(LOG_TRACE, "collective", "reduce %d round %d recv from %d\n",
                  seqnum, i, target_rank);
      post_recv_x(target_rank, tmp_buffer, item_size * count, seqnum, COMP_NULL)
          .runtime(runtime)
          .device(device)
          .endpoint(endpoint)
          .matching_engine(matching_engine)();
      // fprintf(stderr, "rank %d reduce %d round %d recv %lu from %d\n",
      // rank, seqnum, i, *(uint64_t*)tmp_buffer, target_rank);
      const void* right_buffer = data_buffer;
      if (!has_received) {
        has_received = true;
        right_buffer = sendbuf;
        // fprintf(stderr, "rank %d reduce %d round %d right=sendbuf %lu\n",
        // rank, seqnum, i, *(uint64_t*)right_buffer);
        // } else {
        // fprintf(stderr, "rank %d reduce %d round %d right=data_buffer %lu\n",
        // rank, seqnum, i, *(uint64_t*)right_buffer);
      }
      op(tmp_buffer, right_buffer, data_buffer, count);
      // fprintf(stderr, "rank %d reduce %d round %d current data %lu\n",
      // rank, seqnum, i, *(uint64_t*)data_buffer);
    }
  }
  if (to_free_tmp_buffer) free(tmp_buffer);
  if (to_free_data_buffer) free(data_buffer);
  LCI_DBG_Log(LOG_TRACE, "collective",
              "leave reduce %d (root %d buffer %p item_size %lu n %lu)\n",
              seqnum, root, tmp_buffer, item_size, count);
}

}  // namespace lci