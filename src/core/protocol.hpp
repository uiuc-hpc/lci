#ifndef LCI_CORE_PROTOCOL_HPP
#define LCI_CORE_PROTOCOL_HPP

namespace lci
{
enum class packet_data_rdv_type_t { single_2sided, single_1sided, iovec };

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
  packet_t* packet = nullptr;    // 8 bytes
  int rank;                      // 4 bytes
  tag_t tag;                     // 4 bytes
  void* buffer;                  // 8 bytes
  size_t size;                   // 8 bytes
  comp_t comp;                   // 8 bytes
  void* user_context = nullptr;  // 8 bytes

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

}  // namespace lci

#endif  // LCI_CORE_PROTOCOL_HPP