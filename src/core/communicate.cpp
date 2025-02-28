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

status_t post_comm_x::call_impl(
    direction_t direction, int rank, void* local_buffer, size_t size,
    comp_t local_comp, runtime_t runtime, packet_pool_t packet_pool,
    net_endpoint_t net_endpoint, matching_engine_t matching_engine, mr_t mr,
    void* remote_buffer, tag_t tag, rcomp_t remote_comp, void* ctx,
    buffers_t buffers, rbuffers_t rbuffers, bool allow_ok, bool force_rdv) const
{
  packet_t* packet = nullptr;
  internal_context_t* internal_ctx = nullptr;
  internal_context_t* rts_ctx = nullptr;
  bool user_provided_packet = false;

  net_device_t net_device = net_endpoint.p_impl->net_device;
  net_context_t net_context = net_device.p_impl->net_context;
  status_t status;
  status.rank = rank;
  status.tag = tag;
  status.user_context = ctx;
  error_t& error = status.error;
  // allocate internal status object
  internal_ctx = new internal_context_t;
  internal_ctx->rank = rank;
  internal_ctx->tag = tag;
  internal_ctx->comp =
      comp_t();  // eager protocol do not need a local completion object
  internal_ctx->user_context = ctx;

  // determine data type
  data_t data;
  if (buffers.empty()) {
    data = data_t(buffer_t(local_buffer, size, mr));
  } else if (!buffers.empty()) {
    LCI_Assert(local_buffer == nullptr, "The local buffer should be nullptr\n");
    LCI_Assert(size == 0, "The size should be 0\n");
    LCI_Assert(rbuffers.empty() || buffers.size() == rbuffers.size(),
               "The number of buffers and rbuffers should be the same\n");
    data = data_t(buffers);
  }
  status.data = data;
  internal_ctx->data = data;
  if (direction == direction_t::OUT) {
    // send
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

    // determine protocol
    enum class protocol_t {
      inject,
      eager,
      rendezvous,
    } protocol;
    if (!buffers.empty() || force_rdv || size > max_eager_size) {
      protocol = protocol_t::rendezvous;
    } else if (size <= max_inject_size && !force_rdv) {
      protocol = protocol_t::inject;
    } else {
      protocol = protocol_t::eager;
    }
    // set local completion object
    // For now only the rendezvous protocol needs to signal the local completion
    // object
    if (protocol == protocol_t::rendezvous) {
      internal_ctx->comp = local_comp;
    }
    // set immediate data
    uint32_t imm_data = 0;
    static_assert(IMM_DATA_MSG_EAGER == 0);
    if (protocol != protocol_t::rendezvous &&
        tag <= runtime.get_attr_max_imm_tag() &&
        remote_comp <= runtime.get_attr_max_imm_rcomp()) {
      // is_fastpath (1) ; remote_comp (15) ; tag (16)
      imm_data = set_bits32(imm_data, 1, 1, 31);  // is_fastpath
      imm_data = set_bits32(imm_data, tag, runtime.get_attr_imm_nbits_tag(), 0);
      imm_data = set_bits32(imm_data, remote_comp,
                            runtime.get_attr_imm_nbits_rcomp(), 16);
    } else {
      // bit 29-30: imm_data_msg_type_t
      if (protocol == protocol_t::rendezvous) {
        imm_data = set_bits32(0, IMM_DATA_MSG_RTS, 2, 29);
      } else {
        throw std::logic_error("Not implemented");
      }
    }

    if (protocol == protocol_t::inject) {
      // inject protocol, return retry or ok
      error =
          net_endpoint.p_impl->post_sends(rank, local_buffer, size, imm_data);
      goto exit;
    } else if (protocol == protocol_t::eager) {
      // eager protocol, return retry or ok
      // get a packet
      if (packet_pool.p_impl->is_packet(local_buffer)) {
        // users provide a packet
        user_provided_packet = true;
        packet = address2packet(local_buffer);
      } else {
        // allocate a packet
        packet = packet_pool.p_impl->get();
        if (!packet) {
          error.reset(errorcode_t::retry_nopacket);
          goto exit_retry;
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
        goto exit_retry;
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

      // TODO wait for backlog queue

      // build the rts message
      size_t rts_size = rts_msg_t::get_size(data);
      rts_msg_t plain_rts;
      rts_msg_t* p_rts;
      if (rts_size <= max_inject_size) {
        p_rts = &plain_rts;
      } else {
        LCI_Assert(rts_size <= max_eager_size,
                   "The rts message is too large\n");
        packet = packet_pool.p_impl->get();
        if (!packet) {
          error.reset(errorcode_t::retry_nopacket);
          goto exit_retry;
          p_rts = static_cast<rts_msg_t*>(packet->get_payload_address());
        }
        rts_ctx = new internal_context_t;
        rts_ctx->packet = packet;
      }
      p_rts->send_ctx = (uintptr_t)internal_ctx;
      p_rts->rdv_type = rdv_type_t::single;
      p_rts->tag = tag;
      p_rts->rcomp = remote_comp;
      if (local_buffer) {
        p_rts->load_buffer(size);
      } else {
        p_rts->load_buffers(buffers);
      }
      // post send for the rts message
      if (rts_size <= max_inject_size) {
        error =
            net_endpoint.p_impl->post_sends(rank, p_rts, rts_size, imm_data);
      } else {
        error = net_endpoint.p_impl->post_send(rank, p_rts, rts_size,
                                               packet->get_mr(net_device),
                                               imm_data, rts_ctx);
      }
      if (error.is_ok()) {
        error.reset(errorcode_t::posted);
      }
      if (error.is_retry()) {
        goto exit_retry;
      } else {
        LCI_Assert(error.is_posted(), "Unexpected error value\n");
        goto exit;
      }
    }
  } else {
    // recv
    status.error.reset(errorcode_t::posted);
    internal_ctx->comp = local_comp;
    auto key = matching_engine.get_impl()->make_key(rank, tag);
    auto ret = matching_engine.get_impl()->insert(
        key, internal_ctx, matching_engine_impl_t::type_t::recv);
    if (ret) {
      handle_matched_sendrecv(runtime, net_endpoint,
                              reinterpret_cast<packet_t*>(ret), internal_ctx,
                              &status);
    }
    goto exit;
  }

exit_retry:
  if (!user_provided_packet && packet) {
    packet->put_back();
  }
  delete rts_ctx;
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
  return std::move(status);
}

status_t post_am_x::call_impl(int rank, void* local_buffer, size_t size,
                              comp_t local_comp, rcomp_t remote_comp,
                              runtime_t runtime, packet_pool_t packet_pool,
                              net_endpoint_t net_endpoint, mr_t mr, tag_t tag,
                              void* ctx, buffers_t buffers, bool allow_ok,
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
      .buffers(buffers)
      .allow_ok(allow_ok)
      .force_rdv(force_rdv)();
}

status_t post_send_x::call_impl(int rank, void* local_buffer, size_t size,
                                comp_t local_comp, runtime_t runtime,
                                packet_pool_t packet_pool,
                                net_endpoint_t net_endpoint,
                                matching_engine_t matching_engine, mr_t mr,
                                tag_t tag, void* ctx, buffers_t buffers,
                                bool allow_ok, bool force_rdv) const
{
  return post_comm_x(direction_t::OUT, rank, local_buffer, size, local_comp)
      .runtime(runtime)
      .packet_pool(packet_pool)
      .net_endpoint(net_endpoint)
      .matching_engine(matching_engine)
      .mr(mr)
      .tag(tag)
      .ctx(ctx)
      .buffers(buffers)
      .allow_ok(allow_ok)
      .force_rdv(force_rdv)();
}

status_t post_recv_x::call_impl(int rank, void* local_buffer, size_t size,
                                comp_t local_comp, runtime_t runtime,
                                packet_pool_t packet_pool,
                                net_endpoint_t net_endpoint,
                                matching_engine_t matching_engine, mr_t mr,
                                tag_t tag, void* ctx, buffers_t buffers,
                                bool allow_ok, bool force_rdv) const
{
  return post_comm_x(direction_t::IN, rank, local_buffer, size, local_comp)
      .runtime(runtime)
      .packet_pool(packet_pool)
      .net_endpoint(net_endpoint)
      .matching_engine(matching_engine)
      .mr(mr)
      .tag(tag)
      .ctx(ctx)
      .buffers(buffers)
      .allow_ok(allow_ok)
      .force_rdv(force_rdv)();
}

}  // namespace lci