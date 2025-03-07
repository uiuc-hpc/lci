#include "lci_internal.hpp"

namespace lci
{
enum class protocol_t {
  inject,
  bcopy,
  zcopy,
};

status_t post_comm_x::call_impl(
    direction_t direction, int rank, void* local_buffer, size_t size,
    comp_t local_comp, runtime_t runtime, packet_pool_t packet_pool,
    endpoint_t endpoint, matching_engine_t matching_engine,
    out_comp_type_t out_comp_type, mr_t mr, uintptr_t remote_buffer,
    rkey_t rkey, tag_t tag, rcomp_t remote_comp, void* ctx, buffers_t buffers,
    rbuffers_t rbuffers, matching_policy_t matching_policy, bool allow_ok,
    bool allow_retry, bool force_zero_copy) const
{
  // forward delcarations
  packet_t* packet = nullptr;
  internal_context_t* internal_ctx = nullptr;
  internal_context_t* rts_ctx = nullptr;
  bool user_provided_packet = false;

  // basic resources
  device_t device = endpoint.p_impl->device;
  net_context_t net_context = device.p_impl->net_context;

  // basic checks
  bool local_buffer_only = !remote_buffer && rbuffers.empty();

  // get the matching engine rhandler
  if (remote_comp == 0 && !matching_engine.is_empty() && local_buffer_only) {
    // this is send-recv (no remote_comp, no remote_buffer)
    remote_comp = matching_engine.get_impl()->get_rhandler(matching_policy);
  }

  // process COMP_BLOCK
  bool is_local_comp_null = false;
  if (local_comp == COMP_NULL) {
    is_local_comp_null = true;
    local_comp = alloc_sync();
  }

  status_t status;
  status.rank = rank;
  status.tag = tag;
  status.user_context = ctx;
  error_t& error = status.error;
  internal_ctx = new internal_context_t;
  internal_ctx->rank = rank;
  internal_ctx->tag = tag;
  internal_ctx->user_context = ctx;

  // get protocol
  size_t max_inject_size = get_max_inject_size_x()
                               .runtime(runtime)
                               .endpoint(endpoint)
                               .tag(tag)
                               .remote_comp(remote_comp)();
  size_t max_bcopy_size = get_max_bcopy_size_x()
                              .runtime(runtime)
                              .endpoint(endpoint)
                              .packet_pool(packet_pool)
                              .tag(tag)
                              .remote_comp(remote_comp)();
  protocol_t protocol;
  if (!buffers.empty() || force_zero_copy || size > max_bcopy_size) {
    protocol = protocol_t::zcopy;
  } else if (size <= max_inject_size && direction == direction_t::OUT &&
             out_comp_type == out_comp_type_t::buffer) {
    protocol = protocol_t::inject;
  } else {
    protocol = protocol_t::bcopy;
  }
  bool is_rendezvous = protocol == protocol_t::zcopy && local_buffer_only;

  /**********************************************************************************
   * build the data descriptor
   **********************************************************************************/
  data_t& data = internal_ctx->data;
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

  /**********************************************************************************
   * Get a packet
   **********************************************************************************/
  // all buffer-copy ops but recv need a packet
  if (protocol == protocol_t::bcopy &&
      !(direction == direction_t::IN && local_buffer_only)) {
    // get a packet
    if (packet_pool.p_impl->is_packet(local_buffer)) {
      // users provide a packet
      user_provided_packet = true;
      packet = address2packet(local_buffer);
    } else {
      // allocate a packet
      packet = packet_pool.p_impl->get(!allow_retry);
      if (!packet) {
        error = errorcode_t::retry_nopacket;
        goto exit;
      }
      if (direction == direction_t::OUT) {
        memcpy(packet->get_payload_address(), local_buffer, size);
        if (size > runtime.get_attr_packet_return_threshold()) {
          // A lot of data has been written into this packet, which means a
          // large chunk of cache lines have been touched. We should return this
          // packet to the current core's packet pool.
          // TODO: I am skeptical about how much performance gain we can get
          // from this. We should do some experiments to verify this.
          packet->local_context.local_id =
              packet_pool.get_impl()->get_local_id();
        }
      }
    }
    internal_ctx->packet_to_free = packet;
  }

  if (direction == direction_t::OUT) {
    /**********************************************************************************
     * direction out
     **********************************************************************************/
    // wait for the backlog queue to be empty
    if (!endpoint.get_impl()->is_backlog_queue_empty() && allow_retry) {
      LCI_PCOUNTER_ADD(retry_due_to_backlog_queue, 1);
      error = errorcode_t::retry_backlog;
      goto exit;
    }

    // set local completion object
    if (protocol == protocol_t::zcopy ||
        out_comp_type == out_comp_type_t::network) {
      internal_ctx->comp = local_comp;
    }

    // set immediate data
    uint32_t imm_data = 0;
    static_assert(IMM_DATA_MSG_EAGER == 0);
    if (!is_rendezvous && tag <= runtime.get_attr_max_imm_tag() &&
        remote_comp <= runtime.get_attr_max_imm_rcomp()) {
      // is_fastpath (1) ; remote_comp (15) ; tag (16)
      imm_data = set_bits32(imm_data, 1, 1, 31);  // is_fastpath
      imm_data = set_bits32(imm_data, tag, runtime.get_attr_imm_nbits_tag(), 0);
      imm_data = set_bits32(imm_data, remote_comp,
                            runtime.get_attr_imm_nbits_rcomp(), 16);
    } else {
      // bit 29-30: imm_data_msg_type_t
      if (is_rendezvous) {
        imm_data = set_bits32(0, IMM_DATA_MSG_RTS, 2, 29);
      } else {
        throw std::logic_error("Not implemented");
      }
    }

    if (protocol == protocol_t::inject) {
      // inject protocol (return retry or ok)
      if (!remote_buffer) {
        error = endpoint.p_impl->post_sends(rank, local_buffer, size, imm_data,
                                            allow_retry);
      } else if (!remote_comp) {
        // rdma write
        error = endpoint.p_impl->post_puts(
            rank, local_buffer, size,
            reinterpret_cast<uintptr_t>(remote_buffer), 0, rkey, allow_retry);
      } else {
        // rdma write with immediate data
        error = endpoint.p_impl->post_putImms(rank, local_buffer, size,
                                              remote_buffer, 0, rkey, imm_data,
                                              allow_retry);
      }
      // end of inject protocol
    } else if (protocol == protocol_t::bcopy) {
      // buffer-copy protocol
      if (!remote_buffer) {
        // buffer-copy send
        error = endpoint.p_impl->post_send(rank, packet->get_payload_address(),
                                           size, packet->get_mr(device),
                                           imm_data, internal_ctx, allow_retry);
      } else if (!remote_comp) {
        // buffer-copy put
        error = endpoint.p_impl->post_put(
            rank, packet->get_payload_address(), size, packet->get_mr(device),
            reinterpret_cast<uintptr_t>(remote_buffer), 0, rkey, internal_ctx,
            allow_retry);
      } else {
        // buffer-copy put with signal
        error = endpoint.p_impl->post_putImm(
            rank, packet->get_payload_address(), size, packet->get_mr(device),
            reinterpret_cast<uintptr_t>(remote_buffer), 0, rkey, imm_data,
            internal_ctx, allow_retry);
      }
      if (error.is_posted() && out_comp_type == out_comp_type_t::buffer) {
        error = errorcode_t::ok;
      }
      // end of eager protocol
    } else /* protocol == protocol_t::zcopy */ {
      if (!local_buffer_only) {
        // zero-copy put
        if (mr.is_empty()) {
          internal_ctx->mr_on_the_fly =
              register_data(internal_ctx->data, device);
        }
        if (data.is_buffer()) {
          if (!remote_comp) {
            error = endpoint.p_impl->post_put(
                rank, data.buffer.base, data.buffer.size, data.buffer.mr,
                reinterpret_cast<uintptr_t>(remote_buffer), 0, rkey,
                internal_ctx, allow_retry);
          } else {
            error = endpoint.p_impl->post_putImm(
                rank, data.buffer.base, data.buffer.size, data.buffer.mr,
                reinterpret_cast<uintptr_t>(remote_buffer), 0, rkey, imm_data,
                internal_ctx, allow_retry);
          }
        } else {
          internal_context_extended_t* extended_ctx =
              new internal_context_extended_t;
          extended_ctx->internal_ctx = internal_ctx;
          extended_ctx->signal_count = data.get_buffers_count();
          for (size_t i = 0; i < data.buffers.count; i++) {
            if (i > 0) allow_retry = false;
            if (i == data.buffers.count - 1 && remote_comp) {
              // last buffer, use RDMA write with immediate data
              error = endpoint.p_impl->post_putImm(
                  rank, data.buffers.buffers[i].base,
                  data.buffers.buffers[i].size, data.buffers.buffers[i].mr,
                  reinterpret_cast<uintptr_t>(rbuffers[i].base), 0,
                  rbuffers[i].rkey, imm_data, extended_ctx, allow_retry);
            } else {
              error = endpoint.p_impl->post_put(
                  rank, data.buffers.buffers[i].base,
                  data.buffers.buffers[i].size, data.buffers.buffers[i].mr,
                  reinterpret_cast<uintptr_t>(rbuffers[i].base), 0,
                  rbuffers[i].rkey, extended_ctx, allow_retry);
            }
            if (i == 0 && error.is_retry()) {
              goto exit;
            }
            LCI_Assert(error.is_posted(), "Unexpected error %d\n", error);
          }
        }
        // end of zero-copy put
      } else /* local_buffer_only */ {
        // rendezvous send
        // build the rts message
        size_t rts_size = rts_msg_t::get_size(data);
        rts_msg_t plain_rts;
        rts_msg_t* p_rts;
        if (rts_size <= max_inject_size) {
          p_rts = &plain_rts;
        } else {
          LCI_Assert(rts_size <= max_bcopy_size,
                     "The rts message is too large\n");
          packet = packet_pool.p_impl->get(!allow_retry);
          if (!packet) {
            error = errorcode_t::retry_nopacket;
            goto exit;
          }
          p_rts = static_cast<rts_msg_t*>(packet->get_payload_address());
          rts_ctx = new internal_context_t;
          rts_ctx->packet_to_free = packet;
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
          error = endpoint.p_impl->post_sends(rank, p_rts, rts_size, imm_data,
                                              allow_retry);
        } else {
          error = endpoint.p_impl->post_send(rank, p_rts, rts_size,
                                             packet->get_mr(device), imm_data,
                                             rts_ctx, allow_retry);
        }
        if (error.is_ok()) {
          error = errorcode_t::posted;
        }
        // end of rendezvous send
      }
      // end of zero-copy protocol
    }
    // end of direction out
  } else {
    /**********************************************************************************
     * direction in
     **********************************************************************************/
    status.error = errorcode_t::posted;
    internal_ctx->comp = local_comp;
    if (local_buffer_only) {
      // recv
      LCI_DBG_Assert(internal_ctx->packet_to_free == nullptr,
                     "recv does not need a packet!\n");
      // get the matching policy
      // If any of the LCI_ANY is used, we will ignore the matching policy
      if (rank == ANY_SOURCE && tag == ANY_TAG) {
        matching_policy = matching_policy_t::none;
      } else if (rank == ANY_SOURCE) {
        matching_policy = matching_policy_t::tag_only;
      } else if (tag == ANY_TAG) {
        matching_policy = matching_policy_t::rank_only;
      }
      auto key =
          matching_engine.get_impl()->make_key(rank, tag, matching_policy);
      auto ret = matching_engine.get_impl()->insert(
          key, internal_ctx, matching_engine_impl_t::insert_type_t::recv);
      if (ret) {
        handle_matched_sendrecv(runtime, endpoint,
                                reinterpret_cast<packet_t*>(ret), internal_ctx,
                                &status);
      }
    } else /* !local_buffer_only */ {
      // get
      LCI_Assert(remote_comp == 0,
                 "get with signal has not been implemented. We are actively "
                 "searching for use case. Open a github issue.\n");
      if (protocol == protocol_t::bcopy) {
        // buffer-copy
        error = endpoint.p_impl->post_get(
            rank, packet->get_payload_address(), size, packet->get_mr(device),
            reinterpret_cast<uintptr_t>(remote_buffer), 0, rkey, internal_ctx,
            allow_retry);
      } else {
        // zero-copy
        if (mr.is_empty()) {
          internal_ctx->mr_on_the_fly =
              register_data(internal_ctx->data, device);
        }
        if (data.is_buffer()) {
          error = endpoint.p_impl->post_get(
              rank, data.buffer.base, data.buffer.size, data.buffer.mr,
              reinterpret_cast<uintptr_t>(remote_buffer), 0, rkey, internal_ctx,
              allow_retry);
        } else {
          internal_context_extended_t* extended_ctx =
              new internal_context_extended_t;
          extended_ctx->internal_ctx = internal_ctx;
          extended_ctx->signal_count = data.get_buffers_count();
          for (size_t i = 0; i < data.buffers.count; i++) {
            if (i > 0) allow_retry = false;
            error = endpoint.p_impl->post_get(
                rank, data.buffers.buffers[i].base,
                data.buffers.buffers[i].size, data.buffers.buffers[i].mr,
                reinterpret_cast<uintptr_t>(rbuffers[i].base), 0,
                rbuffers[i].rkey, extended_ctx, allow_retry);
            if (i == 0 && error.is_retry()) {
              goto exit;
            } else {
              LCI_Assert(error.is_posted(), "Unexpected error %d\n", error);
            }
          }
        }
      }
    }
    // end of direction in
  }

exit:
  if (error.is_retry()) {
    LCI_DBG_Assert(allow_retry, "Unexpected retry\n");
    if (internal_ctx->mr_on_the_fly) {
      deregister_data(internal_ctx->data);
    }
    if (!user_provided_packet && packet) {
      packet->put_back();
    }
    delete rts_ctx;
    delete internal_ctx;
  }
  if (error.is_posted() && is_local_comp_null) {
    while (!sync_test(local_comp, &status)) {
      progress_x().runtime(runtime).device(device).endpoint(endpoint)();
    }
    free_comp(&local_comp);
    error = errorcode_t::ok;
  }
  if (error.is_ok() && !allow_ok) {
    lci::comp_signal(local_comp, status);
    status.error = errorcode_t::posted;
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
                              endpoint_t endpoint,
                              out_comp_type_t out_comp_type, mr_t mr, tag_t tag,
                              void* ctx, buffers_t buffers, bool allow_ok,
                              bool allow_retry, bool force_zero_copy) const
{
  return post_comm_x(direction_t::OUT, rank, local_buffer, size, local_comp)
      .runtime(runtime)
      .packet_pool(packet_pool)
      .endpoint(endpoint)
      .matching_engine(matching_engine_t())
      .out_comp_type(out_comp_type)
      .mr(mr)
      .tag(tag)
      .remote_comp(remote_comp)
      .ctx(ctx)
      .buffers(buffers)
      .allow_ok(allow_ok)
      .allow_retry(allow_retry)
      .force_zero_copy(force_zero_copy)();
}

status_t post_send_x::call_impl(
    int rank, void* local_buffer, size_t size, tag_t tag, comp_t local_comp,
    runtime_t runtime, packet_pool_t packet_pool, endpoint_t endpoint,
    matching_engine_t matching_engine, out_comp_type_t out_comp_type, mr_t mr,
    void* ctx, buffers_t buffers, matching_policy_t matching_policy,
    bool allow_ok, bool allow_retry, bool force_zero_copy) const
{
  return post_comm_x(direction_t::OUT, rank, local_buffer, size, local_comp)
      .runtime(runtime)
      .packet_pool(packet_pool)
      .endpoint(endpoint)
      .matching_engine(matching_engine)
      .out_comp_type(out_comp_type)
      .mr(mr)
      .tag(tag)
      .ctx(ctx)
      .buffers(buffers)
      .matching_policy(matching_policy)
      .allow_ok(allow_ok)
      .allow_retry(allow_retry)
      .force_zero_copy(force_zero_copy)();
}

status_t post_recv_x::call_impl(int rank, void* local_buffer, size_t size,
                                tag_t tag, comp_t local_comp, runtime_t runtime,
                                packet_pool_t packet_pool, endpoint_t endpoint,
                                matching_engine_t matching_engine, mr_t mr,
                                void* ctx, buffers_t buffers,
                                matching_policy_t matching_policy,
                                bool allow_ok, bool allow_retry,
                                bool force_zero_copy) const
{
  return post_comm_x(direction_t::IN, rank, local_buffer, size, local_comp)
      .runtime(runtime)
      .packet_pool(packet_pool)
      .endpoint(endpoint)
      .matching_engine(matching_engine)
      .mr(mr)
      .tag(tag)
      .ctx(ctx)
      .buffers(buffers)
      .matching_policy(matching_policy)
      .allow_ok(allow_ok)
      .allow_retry(allow_retry)
      .force_zero_copy(force_zero_copy)();
}

status_t post_put_x::call_impl(int rank, void* local_buffer, size_t size,
                               comp_t local_comp, uintptr_t remote_buffer,
                               rkey_t rkey, runtime_t runtime,
                               packet_pool_t packet_pool, endpoint_t endpoint,
                               out_comp_type_t out_comp_type, mr_t mr,
                               tag_t tag, rcomp_t remote_comp, void* ctx,
                               buffers_t buffers, rbuffers_t rbuffers,
                               bool allow_ok, bool allow_retry,
                               bool force_zero_copy) const
{
  return post_comm_x(direction_t::OUT, rank, local_buffer, size, local_comp)
      .remote_buffer(remote_buffer)
      .rkey(rkey)
      .runtime(runtime)
      .packet_pool(packet_pool)
      .endpoint(endpoint)
      .matching_engine(matching_engine_t())
      .out_comp_type(out_comp_type)
      .mr(mr)
      .tag(tag)
      .remote_comp(remote_comp)
      .ctx(ctx)
      .buffers(buffers)
      .rbuffers(rbuffers)
      .allow_ok(allow_ok)
      .allow_retry(allow_retry)
      .force_zero_copy(force_zero_copy)();
}

status_t post_get_x::call_impl(int rank, void* local_buffer, size_t size,
                               comp_t local_comp, uintptr_t remote_buffer,
                               rkey_t rkey, runtime_t runtime,
                               packet_pool_t packet_pool, endpoint_t endpoint,
                               mr_t mr, tag_t tag, rcomp_t remote_comp,
                               void* ctx, buffers_t buffers,
                               rbuffers_t rbuffers, bool allow_ok,
                               bool allow_retry, bool force_zero_copy) const
{
  return post_comm_x(direction_t::IN, rank, local_buffer, size, local_comp)
      .remote_buffer(remote_buffer)
      .rkey(rkey)
      .runtime(runtime)
      .packet_pool(packet_pool)
      .endpoint(endpoint)
      .matching_engine(matching_engine_t())
      .mr(mr)
      .tag(tag)
      .remote_comp(remote_comp)
      .ctx(ctx)
      .buffers(buffers)
      .rbuffers(rbuffers)
      .allow_ok(allow_ok)
      .allow_retry(allow_retry)
      .force_zero_copy(force_zero_copy)();
}

size_t get_max_inject_size_x::call_impl(runtime_t runtime, endpoint_t endpoint,
                                        tag_t tag, rcomp_t remote_comp) const
{
  size_t net_max_inject_size =
      endpoint.p_impl->device.p_impl->net_context.get_attr_max_inject_size();
  return net_max_inject_size;
}

size_t get_max_bcopy_size_x::call_impl(runtime_t runtime, endpoint_t endpoint,
                                       packet_pool_t packet_pool, tag_t tag,
                                       rcomp_t remote_comp) const
{
  return packet_pool.p_impl->get_payload_size();
}

}  // namespace lci