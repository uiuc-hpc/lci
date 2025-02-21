#include "lci_internal.hpp"

namespace lci
{
size_t get_max_inject_size_x::call_impl(runtime_t runtime,
                                        net_endpoint_t net_endpoint, tag_t tag,
                                        rcomp_t remote_comp) const
{
  size_t net_max_inject_size = net_endpoint.p_impl->net_device.p_impl
                                   ->net_context.get_attr_max_inject_size();
  return net_max_inject_size;
}

size_t get_max_eager_size_x::call_impl(runtime_t runtime,
                                       net_endpoint_t net_endpoint,
                                       packet_pool_t packet_pool, tag_t tag,
                                       rcomp_t remote_comp) const
{
  return packet_pool.p_impl->get_pmessage_size();
}

status_t post_comm_x::call_impl(direction_t direction, int rank,
                                void* local_buffer, size_t size,
                                comp_t local_comp, runtime_t runtime,
                                packet_pool_t packet_pool,
                                net_endpoint_t net_endpoint, mr_t mr,
                                void* remote_buffer, tag_t tag,
                                rcomp_t remote_comp, void* ctx, bool allow_ok,
                                bool assert_eager, bool force_rdv) const
{
  net_device_t net_device = net_endpoint.p_impl->net_device;
  net_context_t net_context = net_device.p_impl->net_context;
  status_t status;
  status.buffer = local_buffer;
  status.size = size;
  status.rank = rank;
  status.tag = tag;
  status.user_context = ctx;
  error_t& error = status.error;
  // allocate internal status object
  internal_context_t* internal_ctx = new internal_context_t;
  internal_ctx->rank = rank;
  internal_ctx->tag = tag;
  internal_ctx->buffer = local_buffer;
  internal_ctx->size = size;
  internal_ctx->comp =
      comp_t();  // eager protocol do not need a local completion object
  internal_ctx->user_context = ctx;
  internal_ctx->mr = mr;

  packet_t* packet = nullptr;
  bool user_provided_packet = false;
  if (direction == direction_t::OUT) {
    size_t max_inject_size = get_max_inject_size_x()
                                 .runtime(runtime)
                                 .net_endpoint(net_endpoint)
                                 .tag(tag)
                                 .remote_comp(remote_comp)();
    size_t max_eager_size = get_max_eager_size_x()
                                .runtime(runtime)
                                .net_endpoint(net_endpoint)
                                .packet_pool(packet_pool)
                                .tag(tag)
                                .remote_comp(remote_comp)();
    // set immediate data
    uint32_t imm_data = 0;
    static_assert(IMM_DATA_MSG_EAGER == 0);
    if (size <= max_eager_size && tag <= runtime.get_attr_max_imm_tag() &&
        remote_comp <= runtime.get_attr_max_imm_rcomp() && !force_rdv) {
      // is_fastpath (1) ; remote_comp (15) ; tag (16)
      imm_data = set_bits32(imm_data, 1, 1, 31);  // is_fastpath
      imm_data = set_bits32(imm_data, tag, runtime.get_attr_imm_nbits_tag(), 0);
      imm_data = set_bits32(imm_data, remote_comp,
                            runtime.get_attr_imm_nbits_rcomp(), 16);
    } else {
      imm_data = 0;
      // bit 29-30: imm_data_msg_type_t
      if (size > max_eager_size || force_rdv) {
        imm_data = set_bits32(imm_data, IMM_DATA_MSG_RTS, 2, 29);
      } else {
        throw std::logic_error("Not implemented");
      }
    }

    if (size <= max_inject_size && !force_rdv) {
      // fast path
      error =
          net_endpoint.p_impl->post_sends(rank, local_buffer, size, imm_data);
      goto exit;
    } else if (size <= max_eager_size && !force_rdv) {
      // eager protocol
      // get a packet
      if (packet_pool.p_impl->is_packet(local_buffer)) {
        // users provide a packet
        user_provided_packet = true;
        packet = address2packet(local_buffer);
        internal_ctx->buffer = nullptr;  // users do not own the packet anymore
      } else {
        // allocate a packet
        packet = packet_pool.p_impl->get();
        if (!packet) {
          error.reset(errorcode_t::retry_nomem);
          goto exit_free_ctx;
        }
        memcpy(packet->get_payload_address(), local_buffer, size);
      }
      packet->local_context.local_id =
          (size > runtime.get_attr_packet_return_threshold())
              ? packet_pool.p_impl->pool.get_local_set_id()
              : mpmc_set_t::LOCAL_SET_ID_NULL;
      internal_ctx->packet = packet;
      // post send
      error = net_endpoint.p_impl->post_send(
          rank, packet->get_payload_address(), size, packet->get_mr(net_device),
          imm_data, internal_ctx);
      if (error.is_retry()) {
        goto exit_free_packet;
      } else {
        LCI_Assert(error.is_posted() || error.is_ok(),
                   "Unexpected error value\n");
        status.error.reset(errorcode_t::ok);
        if (!allow_ok) {
          lci::comp_signal(local_comp, status);
          status.error.reset(errorcode_t::posted);
        }
        goto exit;
      }
    } else {
      // rendezvous protocol
      LCI_Assert(!assert_eager, "We are not using the eager protocol!\n");

      // TODO wait for backlog queue

      internal_ctx->comp = local_comp;
      // build the rts message
      rts_msg_t rts;
      rts.send_ctx = (uintptr_t)internal_ctx;
      rts.rdv_type = rdv_type_t::single_1sided;
      rts.tag = tag;
      rts.rcomp = remote_comp;
      rts.size = size;
      // post send
      LCI_Assert(rts_msg_t::get_size_plain() <= max_inject_size,
                 "The rts message is too large\n");
      error = net_endpoint.p_impl->post_sends(
          rank, &rts, rts_msg_t::get_size_plain(), imm_data);
      if (error.is_ok()) {
        error.reset(errorcode_t::posted);
      }

      if (error.is_retry()) {
        goto exit_free_ctx;
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
  if (!user_provided_packet && packet) {
    packet->put_back();
  }

exit_free_ctx:
  delete internal_ctx;

exit:
  if (error.is_ok() && !allow_ok) {
    lci::comp_signal(local_comp, status);
    status.error.reset(errorcode_t::posted);
  }
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
                              net_endpoint_t net_endpoint, mr_t mr, tag_t tag,
                              void* ctx, bool allow_ok, bool assert_eager,
                              bool force_rdv) const
{
  return post_comm_x(direction_t::OUT, rank, local_buffer, size, local_comp)
      .runtime(runtime)
      .packet_pool(packet_pool)
      .net_endpoint(net_endpoint)
      .mr(mr)
      .tag(tag)
      .remote_comp(remote_comp)
      .ctx(ctx)
      .allow_ok(allow_ok)
      .assert_eager(assert_eager)
      .force_rdv(force_rdv)();
}
}  // namespace lci