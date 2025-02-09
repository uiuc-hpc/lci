#include "lci_internal.hpp"

namespace lci
{
status_t post_comm_x::call_impl(
    direction_t direction, int rank, void* local_buffer, size_t size,
    comp_t local_comp, runtime_t runtime, packet_pool_t packet_pool,
    net_endpoint_t net_endpoint, void* remote_buffer, tag_t tag,
    rcomp_t remote_comp, void* ctx, bool allow_ok) const
{
  net_device_t net_device = net_endpoint.p_impl->net_device;
  net_context_t net_context = net_device.p_impl->context;
  status_t status;
  error_t& error = status.error;
  // allocate internal status object
  internal_context_t* internal_ctx = new internal_context_t;
  internal_ctx->rank = rank;
  internal_ctx->tag = tag;
  internal_ctx->buffer = local_buffer;
  internal_ctx->size = size;
  internal_ctx->comp = local_comp;
  internal_ctx->user_context = ctx;

  packet_t* packet;
  if (direction == direction_t::SEND) {
    bool is_eager = true;
    // set immediate data
    // is_imm (1) ; remote_comp (15) ; tag (16)
    uint32_t imm_data =
        set_bits32(imm_data, tag, runtime.get_attr_imm_nbits_tag(), 0);
    imm_data = set_bits32(imm_data, remote_comp,
                          runtime.get_attr_imm_nbits_rcomp(), 16);
    imm_data = set_bits32(imm_data, is_eager, 1, 31);

    if (size <= net_context.get_attr_max_inject_size()) {
      // fast path
      error =
          net_endpoint.p_impl->post_sends(rank, local_buffer, size, imm_data);
      if (error.is_ok()) {
        status.buffer = local_buffer;
        status.size = size;
        status.rank = rank;
        status.tag = tag;
        status.user_context = ctx;
        if (!allow_ok) {
          lci::comp_signal(local_comp, status);
          status.error.reset(errorcode_t::posted);
        }
      }
      goto exit;
    } else {
      // get a packet
      if (packet_pool.p_impl->is_packet(local_buffer)) {
        // users provide a packet
        packet = address2packet(local_buffer);
        internal_ctx->buffer = nullptr;  // users do not own the packet anymore
      } else {
        // allocate a packet
        packet = packet_pool.p_impl->get();
        if (!packet) {
          error.reset(errorcode_t::retry_nomem);
          goto exit_free_ctx;
        }
        memcpy(packet->get_message_address(), local_buffer, size);
      }
      packet->local_context.local_id =
          (size > runtime.get_attr_packet_return_threshold())
              ? packet_pool.p_impl->pool.get_local_set_id()
              : mpmc_set_t::LOCAL_SET_ID_NULL;
      internal_ctx->packet = packet;
      // post send
      error = net_endpoint.p_impl->post_send(
          rank, packet->get_message_address(), size, packet->get_mr(net_device),
          imm_data, internal_ctx);
      if (error.is_retry()) {
        goto exit_free_packet;
      } else {
        LCI_Assert(error.is_posted(), "Unexpected error value\n");
        goto exit;
      }
    }
  } else {
    // recv
    throw std::logic_error("Not implemented");
  }

exit_free_packet:
  packet->put_back();

exit_free_ctx:
  delete internal_ctx;

exit:
  if (error.is_ok()) {
    LCI_PCOUNTER_ADD(communicate_ok, 1);
  } else if (error.is_posted()) {
    LCI_PCOUNTER_ADD(communicate_posted, 1);
  } else {
    LCI_PCOUNTER_ADD(communicate_retry, 1);
  }
  return status;
}

status_t post_am_x::call_impl(int rank, void* local_buffer, size_t size,
                              comp_t local_comp, rcomp_t remote_comp,
                              runtime_t runtime, packet_pool_t packet_pool,
                              net_endpoint_t net_endpoint, tag_t tag, void* ctx,
                              bool allow_ok) const
{
  return post_comm_x(direction_t::SEND, rank, local_buffer, size, local_comp)
      .runtime(runtime)
      .packet_pool(packet_pool)
      .net_endpoint(net_endpoint)
      .tag(tag)
      .remote_comp(remote_comp)
      .ctx(ctx)
      .allow_ok(allow_ok)();
}
}  // namespace lci