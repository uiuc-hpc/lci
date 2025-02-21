#ifndef LCI_CORE_PROTOCOL_HPP
#define LCI_CORE_PROTOCOL_HPP

namespace lci
{
enum class rdv_type_t { single_2sided, single_1sided, iovec };

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
  // 56 bytes, 4 bit
  // is_extended has to be the first bit (be the same as internal_context_t)
  bool is_extended : 1;          // 1 bit
  bool mr_on_the_fly : 1;        // 1 bit
  rdv_type_t rdv_type : 2;       // 2 bits
  packet_t* packet = nullptr;    // 8 bytes
  int rank;                      // 4 bytes
  tag_t tag;                     // 4 bytes
  void* buffer;                  // 8 bytes
  size_t size;                   // 8 bytes
  comp_t comp;                   // 8 bytes
  void* user_context = nullptr;  // 8 bytes
  mr_t mr;                       // 8 bytes

  internal_context_t()
      : is_extended(false),
        mr_on_the_fly(false),
        rdv_type(rdv_type_t::single_2sided),
        rank(-1),
        tag(0),
        buffer(nullptr),
        size(0),
        comp(comp_t()),
        user_context(nullptr),
        mr(mr_t())
  {
  }

  inline status_t get_status() const
  {
    status_t status;
    status.error = errorcode_t::ok;
    status.rank = rank;
    status.tag = tag;
    status.buffer = buffer;
    status.size = size;
    status.user_context = user_context;
    return status;
  }
};

inline void free_ctx_and_signal_comp(internal_context_t* internal_ctx)
{
  if (internal_ctx->mr_on_the_fly) {
    internal_ctx->mr.get_impl()->deregister();
  }
  if (!internal_ctx->comp.is_empty()) {
    status_t status = internal_ctx->get_status();
    comp_t comp = internal_ctx->comp;
    delete internal_ctx;
    comp.p_impl->signal(status);
  } else {
    delete internal_ctx;
  }
}

struct alignas(LCI_CACHE_LINE) internal_context_extended_t {
  // is_extended has to be the first bit (be the same as internal_context_t)
  bool is_extended : 1;              // 1 bit
  internal_context_t* internal_ctx;  // 8 bytes
  std::atomic<int> signal_count;     // 4 bytes
  uint64_t recv_ctx;                 // 8 bytes

  internal_context_extended_t()
      : is_extended(true), internal_ctx(nullptr), signal_count(0)
  {
  }
};

}  // namespace lci

#endif  // LCI_CORE_PROTOCOL_HPP