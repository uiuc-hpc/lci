// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#ifndef LCI_CORE_PROTOCOL_HPP
#define LCI_CORE_PROTOCOL_HPP

namespace lci
{
enum imm_data_msg_type_t {
  IMM_DATA_MSG_EAGER = 0,
  IMM_DATA_MSG_RTS = 1,
  IMM_DATA_MSG_RTR = 2,
  IMM_DATA_MSG_FIN = 3
};

/**
 * Internal context structure, Used by asynchronous operations to pass
 * information between initialization phase and completion phase.
 * (1) for issue_send->send_completion, go through user_context field of
 * backends (2) for issue_recv->recv_completion, go through the matching table
 * (3) for issue_recvl->issue_rtr->rdma_completion, go through the matching
 * table and register It is also used by rendezvous protocol to pass information
 * between different steps (1) for sending_RTS->sending_WriteImm (long,iovec),
 * go through the send_ctx field of the RTS and RTR messages (2) for
 * sending_RTR->complet_WriteImm (long), go through the ctx_archive. Its key is
 *     passed by the recv_ctx_key field of RTR messages/as meta data of
 * WriteImm. (3) for recving_RTR->complet_All_Write (iovec), go through the
 * ctx_archiveMulti. (4) for sending_RTR->recving_FIN (iovec), go through the
 * recv_ctx field of RTR and FIN messages.
 */
struct packet_t;
struct alignas(LCI_CACHE_LINE) internal_context_t {
  // 52 bytes, 1 bit
  // is_extended has to be the first bit (be the same as internal_context_t)
  bool is_extended : 1;                // 1 bit
  bool mr_on_the_fly : 1;              // 1 bit
  int rank;                            // 4 bytes
  packet_t* packet_to_free = nullptr;  // 8 bytes
  mr_t mr;                             // 8 bytes
  comp_t comp;                         // 8 bytes
  tag_t tag;                           // 8 bytes
  void* buffer = nullptr;              // 8 bytes
  size_t size = 0;                     // 8 bytes
  void* user_context = nullptr;        // 8 bytes

  internal_context_t()
      : is_extended(false),
        mr_on_the_fly(false),
        rank(-1),
        comp(COMP_NULL),
        tag(0),
        user_context(nullptr)
  {
  }

  inline status_t get_status()
  {
    status_t status;
    status.set_done();
    status.rank = rank;
    status.tag = tag;
    status.buffer = buffer;
    status.size = size;
    status.user_context = user_context;
    return status;
  }
};

struct alignas(LCI_CACHE_LINE) internal_context_extended_t {
  // is_extended has to be the first bit (be the same as internal_context_t)
  bool is_extended : 1;              // 1 bit
  internal_context_t* internal_ctx;  // 8 bytes
  std::atomic<int> signal_count;     // 4 bytes
  uint64_t recv_ctx;                 // 8 bytes
  // if set, send imm_data to rank once signal_count reaches 0
  int imm_data_rank;        // 4 bytes
  net_imm_data_t imm_data;  // 4 bytes

  internal_context_extended_t()
      : is_extended(true),
        internal_ctx(nullptr),
        signal_count(0),
        recv_ctx(0),
        imm_data_rank(-1),
        imm_data(0)
  {
  }
};

}  // namespace lci

#endif  // LCI_CORE_PROTOCOL_HPP