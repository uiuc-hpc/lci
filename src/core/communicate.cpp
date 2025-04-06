// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

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
    device_t device, endpoint_t endpoint, matching_engine_t matching_engine,
    comp_semantic_t comp_semantic, mr_t mr, uintptr_t remote_buffer,
    rkey_t rkey, tag_t tag, rcomp_t remote_comp, void* user_context,
    buffers_t buffers, rbuffers_t rbuffers, matching_policy_t matching_policy,
    bool allow_ok, bool allow_posted, bool allow_retry, bool force_zcopy) const
{
  // forward delcarations
  packet_t* packet = nullptr;
  internal_context_t* internal_ctx = nullptr;
  internal_context_t* rts_ctx = nullptr;
  bool user_provided_packet = false;
  bool free_local_comp = false;

  // basic resources
  net_context_t net_context = device.p_impl->net_context;

  // basic conditions
  bool local_buffer_only = !remote_buffer && rbuffers.empty();
  bool local_comp_only = remote_comp == 0;
  bool is_single_buffer = buffers.empty();
  bool is_recv = direction == direction_t::IN && local_buffer_only;
  size_t max_inject_size = net_context.get_attr_max_inject_size();
  size_t max_bcopy_size =
      get_max_bcopy_size_x().runtime(runtime).packet_pool(packet_pool)();

  // basic checks
  // buffer and buffers should not be used at the same time
  if (!buffers.empty()) {
    LCI_Assert(local_buffer == nullptr, "The local buffer should be nullptr\n");
    LCI_Assert(size == 0, "The size should be 0\n");
    LCI_Assert(rbuffers.empty() || buffers.size() == rbuffers.size(),
               "The number of buffers and rbuffers should be the same\n");
  }
  LCI_Assert(
      !(direction == direction_t::IN && local_buffer_only && !local_comp_only),
      "invalid communication primitive\n");
  LCI_Assert(
      !(direction == direction_t::IN && !local_buffer_only && !local_comp_only),
      "get with signal has not been implemented yet\n");

  if (local_comp == COMP_NULL_EXPECT_OK) {
    allow_retry = false;
    allow_posted = false;
  } else if (local_comp == COMP_NULL_EXPECT_OK_OR_RETRY) {
    allow_posted = false;
  }

  // get the matching engine rhandler
  // remote handler can be the user-specified remote completion handler or the
  // matching engine's remote handler
  rcomp_t rhandler = remote_comp;
  if (!rhandler && !matching_engine.is_empty() && local_buffer_only) {
    // this is send-recv (no remote_comp, no remote_buffer)
    rhandler = matching_engine.get_impl()->get_rhandler(matching_policy);
  }

  // setup protocol (part 1): whether to use the zero-copy protocol
  protocol_t protocol = protocol_t::bcopy;
  if (!is_single_buffer || force_zcopy || size > max_bcopy_size) {
    // We use the zero-copy protocol in one of the three cases:
    // 1. The user provides multiple buffers.
    // 2. The user forces to use the zero-copy protocol.
    // 3. The size of the data is larger than the maximum buffer-copy size.
    protocol = protocol_t::zcopy;
  }
  // zero-copy send/am will use the rendezvous ready-to-send message
  bool is_out_rdv = protocol == protocol_t::zcopy && local_buffer_only &&
                    direction == direction_t::OUT;

  // set immediate data
  // immediate data is used for send/am/put with signal
  bool piggyback_tag_rcomp_in_packet = false;
  size_t packet_size_to_send = size;
  uint32_t imm_data = 0;
  if (direction == direction_t::OUT && rhandler) {
    if (!is_out_rdv) {
      static_assert(IMM_DATA_MSG_EAGER == 0, "Unexpected IMM_DATA_MSG_EAGER");
      // Put with signal or eager send/am
      if (tag <= runtime.get_attr_max_imm_tag() &&
          rhandler <= runtime.get_attr_max_imm_rcomp()) {
        // is_fastpath (1) ; rhandler (15) ; tag (16)
        imm_data = set_bits32(imm_data, 1, 1, 31);  // is_fastpath
        imm_data =
            set_bits32(imm_data, tag, runtime.get_attr_imm_nbits_tag(), 0);
        imm_data = set_bits32(imm_data, rhandler,
                              runtime.get_attr_imm_nbits_rcomp(), 16);
      } else if (local_buffer_only) {
        // Eager send/am
        packet_size_to_send += sizeof(tag) + sizeof(rcomp_t);
        piggyback_tag_rcomp_in_packet = true;
      } else {
        LCI_DBG_Assert(remote_comp == rhandler,
                       "Internal error. Report to maintainers\n");
        LCI_Assert(false,
                   "The tag (%lu) or remote completion (%lu) is too large for "
                   "put with signal\n",
                   tag, rhandler);
      }
    } else {
      // Rendezvous send/am
      // bit 29-30: imm_data_msg_type_t
      imm_data = set_bits32(0, IMM_DATA_MSG_RTS, 2, 29);
    }
  }

  // setup protocol (part 2): whether to use the inject protocol
  if (size <= max_inject_size && protocol != protocol_t::zcopy &&
      !piggyback_tag_rcomp_in_packet && direction == direction_t::OUT &&
      comp_semantic == comp_semantic_t::buffer) {
    // We use the inject protocol only if the five conditions are met:
    // 1. The size of the data is smaller than the maximum inject size.
    // 2. The protocol is not zero-copy.
    // 3. The tag and remote completion are not piggybacked in the packet.
    // 4. The direction is OUT.
    // 5. The completion type is buffer.
    protocol = protocol_t::inject;
  }

  // set up some trivial fields for the status and internal context
  data_t data;
  if (buffers.empty()) {
    data = data_t(buffer_t(local_buffer, size, mr));
  } else {
    data = data_t(buffers);
  }
  status_t status;
  status.rank = rank;
  status.tag = tag;
  status.user_context = user_context;
  status.data = data;
  error_t& error = status.error;
  if (protocol != protocol_t::inject) {
    internal_ctx = internal_context_t::alloc();
    internal_ctx->rank = rank;
    internal_ctx->tag = tag;
    internal_ctx->user_context = user_context;
    internal_ctx->data = data;
  }

  /**********************************************************************************
   * Get a packet
   **********************************************************************************/
  // all buffer-copy pritimitves but recv need a packet
  // get a packet
  if (protocol == protocol_t::bcopy && !is_recv) {
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
    }
    internal_ctx->packet_to_free = packet;
  }
  // build the packet
  if (packet && direction == direction_t::OUT) {
    if (!user_provided_packet)
      memcpy(packet->get_payload_address(), local_buffer, size);
    if (piggyback_tag_rcomp_in_packet) {
      LCI_DBG_Assert(
          packet_size_to_send == size + sizeof(tag) + sizeof(rhandler), "");
      LCI_DBG_Assert(packet_size_to_send <= max_bcopy_size, "");
      memcpy((char*)packet->get_payload_address() + size, &tag, sizeof(tag));
      memcpy((char*)packet->get_payload_address() + size + sizeof(tag),
             &rhandler, sizeof(rhandler));
    }
    if (packet_size_to_send > runtime.get_attr_packet_return_threshold()) {
      // A lot of data has been written into this packet, which means a
      // large chunk of cache lines have been touched. We should return this
      // packet to the current core's packet pool.
      // TODO: I am skeptical about how much performance gain we can get
      // from this. We should do some experiments to verify this.
      packet->local_context.local_id = packet_pool.get_impl()->get_local_id();
    }
  }

  // We need to directly return retry if
  // 1. allow_retry is true
  // 2. the endpoint's backlog queue is not empty
  // 3. we are not doing a recv
  if (!endpoint.get_impl()->is_backlog_queue_empty(rank) && allow_retry &&
      !is_recv) {
    LCI_PCOUNTER_ADD(retry_due_to_backlog_queue, 1);
    error = errorcode_t::retry_backlog;
    goto exit;
  }

  // We need to set the local completion object in one of the following cases:
  // 1. The protocol is zero-copy.
  // 2. The completion type is network.
  // 3. The direction is IN (we are doing a recv/get).
  // In other words, eager send/am/put will immediately complete.
  if (protocol == protocol_t::zcopy ||
      comp_semantic == comp_semantic_t::network ||
      direction == direction_t::IN) {
    // process COMP_BLOCK
    if (local_comp == COMP_NULL_EXPECT_OK) {
      local_comp = alloc_sync();
      free_local_comp = true;
    } else if (local_comp == COMP_NULL_EXPECT_OK_OR_RETRY) {
      local_comp = alloc_sync();
      free_local_comp = true;
    }
    internal_ctx->comp = local_comp;
  }

  // We need to have valid memeory regions if all of the following conditions
  // are met:
  // 1. The protocol is zero-copy.
  // 2. We are doing a put/get.
  // Note: mr for zero-copy send/recv will be handled in the rendezvous
  // protocol.
  if (protocol == protocol_t::zcopy && !local_buffer_only && mr.is_empty()) {
    internal_ctx->mr_on_the_fly = register_data(internal_ctx->data, device);
    data = internal_ctx->data;
  }

  if (direction == direction_t::OUT) {
    /**********************************************************************************
     * direction out
     **********************************************************************************/
    if (protocol == protocol_t::inject) {
      // inject protocol (return retry or ok)
      if (!remote_buffer) {
        error = endpoint.p_impl->post_sends(rank, local_buffer, size, imm_data,
                                            allow_retry);
      } else if (!rhandler) {
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
        // note: we need to use packet_size_to_send instead of size
        error = endpoint.p_impl->post_send(
            rank, packet->get_payload_address(), packet_size_to_send,
            packet->get_mr(device), imm_data, internal_ctx, allow_retry);
      } else if (!rhandler) {
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
      if (error.is_posted() && comp_semantic == comp_semantic_t::buffer) {
        error = errorcode_t::ok;
      }
      // end of bcopy protocol
    } else /* protocol == protocol_t::zcopy */ {
      if (!local_buffer_only) {
        // zero-copy put
        if (data.is_buffer()) {
          if (!rhandler) {
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
          auto extended_ctx = internal_context_extended_t::alloc();
          extended_ctx->internal_ctx = internal_ctx;
          extended_ctx->signal_count = data.get_buffers_count();
          for (size_t i = 0; i < data.buffers.count; i++) {
            if (i > 0) allow_retry = false;
            if (i == data.buffers.count - 1 && rhandler) {
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
        rts_msg_t* p_rts;
        if (rts_size <= max_inject_size) {
          p_rts = reinterpret_cast<rts_msg_t*>(malloc(rts_size));
        } else {
          LCI_Assert(rts_size <= max_bcopy_size,
                     "The rts message is too large\n");
          packet = packet_pool.p_impl->get(!allow_retry);
          if (!packet) {
            error = errorcode_t::retry_nopacket;
            goto exit;
          }
          p_rts = static_cast<rts_msg_t*>(packet->get_payload_address());
          rts_ctx = internal_context_t::alloc();
          rts_ctx->packet_to_free = packet;
        }
        p_rts->send_ctx = (uintptr_t)internal_ctx;
        p_rts->rdv_type = rdv_type_t::single;
        p_rts->tag = tag;
        p_rts->rcomp = rhandler;
        if (local_buffer) {
          p_rts->load_buffer(size);
        } else {
          p_rts->load_buffers(buffers);
        }
        // post send for the rts message
        if (rts_size <= max_inject_size) {
          error = endpoint.p_impl->post_sends(rank, p_rts, rts_size, imm_data,
                                              allow_retry);
          free(p_rts);
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
    if (local_buffer_only) {
      // recv
      error = errorcode_t::posted;
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
      LCI_Assert(rhandler == 0,
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
        if (data.is_buffer()) {
          error = endpoint.p_impl->post_get(
              rank, data.buffer.base, data.buffer.size, data.buffer.mr,
              reinterpret_cast<uintptr_t>(remote_buffer), 0, rkey, internal_ctx,
              allow_retry);
        } else {
          auto extended_ctx = internal_context_extended_t::alloc();
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
    if (internal_ctx && internal_ctx->mr_on_the_fly) {
      deregister_data(internal_ctx->data);
    }
    if (!user_provided_packet && packet) {
      packet->put_back();
    }
    internal_context_t::free(rts_ctx);
    internal_context_t::free(internal_ctx);
  }
  if (error.is_posted() && !allow_posted) {
    while (!sync_test(local_comp, &status)) {
      progress_x().runtime(runtime).device(device).endpoint(endpoint)();
    }
    error = errorcode_t::ok;
  }
  if (free_local_comp) {
    free_comp(&local_comp);
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
    switch (error.errorcode) {
      case errorcode_t::retry_backlog:
        LCI_PCOUNTER_ADD(communicate_retry_backlog, 1);
        break;
      case errorcode_t::retry_nopacket:
        LCI_PCOUNTER_ADD(communicate_retry_nopacket, 1);
        break;
      case errorcode_t::retry_nomem:
        LCI_PCOUNTER_ADD(communicate_retry_nomem, 1);
        break;
      case errorcode_t::retry_lock:
        LCI_PCOUNTER_ADD(communicate_retry_lock, 1);
        break;
      default:
        LCI_PCOUNTER_ADD(communicate_retry, 1);
        break;
    }
  }
  return status;
}

status_t post_am_x::call_impl(int rank, void* local_buffer, size_t size,
                              comp_t local_comp, rcomp_t remote_comp,
                              runtime_t runtime, packet_pool_t packet_pool,
                              device_t device, endpoint_t endpoint,
                              comp_semantic_t comp_semantic, mr_t mr, tag_t tag,
                              void* user_context, buffers_t buffers,
                              bool allow_ok, bool allow_posted,
                              bool allow_retry, bool force_zcopy) const
{
  return post_comm_x(direction_t::OUT, rank, local_buffer, size, local_comp)
      .runtime(runtime)
      .packet_pool(packet_pool)
      .device(device)
      .endpoint(endpoint)
      .matching_engine(matching_engine_t())
      .comp_semantic(comp_semantic)
      .mr(mr)
      .tag(tag)
      .remote_comp(remote_comp)
      .user_context(user_context)
      .buffers(buffers)
      .allow_ok(allow_ok)
      .allow_posted(allow_posted)
      .allow_retry(allow_retry)
      .force_zcopy(force_zcopy)();
}

status_t post_send_x::call_impl(
    int rank, void* local_buffer, size_t size, tag_t tag, comp_t local_comp,
    runtime_t runtime, packet_pool_t packet_pool, device_t device,
    endpoint_t endpoint, matching_engine_t matching_engine,
    comp_semantic_t comp_semantic, mr_t mr, void* user_context,
    buffers_t buffers, matching_policy_t matching_policy, bool allow_ok,
    bool allow_posted, bool allow_retry, bool force_zcopy) const
{
  return post_comm_x(direction_t::OUT, rank, local_buffer, size, local_comp)
      .runtime(runtime)
      .packet_pool(packet_pool)
      .device(device)
      .endpoint(endpoint)
      .matching_engine(matching_engine)
      .comp_semantic(comp_semantic)
      .mr(mr)
      .tag(tag)
      .user_context(user_context)
      .buffers(buffers)
      .matching_policy(matching_policy)
      .allow_ok(allow_ok)
      .allow_posted(allow_posted)
      .allow_retry(allow_retry)
      .force_zcopy(force_zcopy)();
}

status_t post_recv_x::call_impl(
    int rank, void* local_buffer, size_t size, tag_t tag, comp_t local_comp,
    runtime_t runtime, packet_pool_t packet_pool, device_t device,
    endpoint_t endpoint, matching_engine_t matching_engine, mr_t mr,
    void* user_context, buffers_t buffers, matching_policy_t matching_policy,
    bool allow_ok, bool allow_posted, bool allow_retry, bool force_zcopy) const
{
  return post_comm_x(direction_t::IN, rank, local_buffer, size, local_comp)
      .runtime(runtime)
      .packet_pool(packet_pool)
      .device(device)
      .endpoint(endpoint)
      .matching_engine(matching_engine)
      .mr(mr)
      .tag(tag)
      .user_context(user_context)
      .buffers(buffers)
      .matching_policy(matching_policy)
      .allow_ok(allow_ok)
      .allow_posted(allow_posted)
      .allow_retry(allow_retry)
      .force_zcopy(force_zcopy)();
}

status_t post_put_x::call_impl(
    int rank, void* local_buffer, size_t size, comp_t local_comp,
    uintptr_t remote_buffer, rkey_t rkey, runtime_t runtime,
    packet_pool_t packet_pool, device_t device, endpoint_t endpoint,
    comp_semantic_t comp_semantic, mr_t mr, tag_t tag, rcomp_t remote_comp,
    void* user_context, buffers_t buffers, rbuffers_t rbuffers, bool allow_ok,
    bool allow_posted, bool allow_retry, bool force_zcopy) const
{
  return post_comm_x(direction_t::OUT, rank, local_buffer, size, local_comp)
      .remote_buffer(remote_buffer)
      .rkey(rkey)
      .runtime(runtime)
      .packet_pool(packet_pool)
      .device(device)
      .endpoint(endpoint)
      .matching_engine(matching_engine_t())
      .comp_semantic(comp_semantic)
      .mr(mr)
      .tag(tag)
      .remote_comp(remote_comp)
      .user_context(user_context)
      .buffers(buffers)
      .rbuffers(rbuffers)
      .allow_ok(allow_ok)
      .allow_posted(allow_posted)
      .allow_retry(allow_retry)
      .force_zcopy(force_zcopy)();
}

status_t post_get_x::call_impl(int rank, void* local_buffer, size_t size,
                               comp_t local_comp, uintptr_t remote_buffer,
                               rkey_t rkey, runtime_t runtime,
                               packet_pool_t packet_pool, device_t device,
                               endpoint_t endpoint, mr_t mr, tag_t tag,
                               rcomp_t remote_comp, void* user_context,
                               buffers_t buffers, rbuffers_t rbuffers,
                               bool allow_ok, bool allow_posted,
                               bool allow_retry, bool force_zcopy) const
{
  return post_comm_x(direction_t::IN, rank, local_buffer, size, local_comp)
      .remote_buffer(remote_buffer)
      .rkey(rkey)
      .runtime(runtime)
      .packet_pool(packet_pool)
      .device(device)
      .endpoint(endpoint)
      .matching_engine(matching_engine_t())
      .mr(mr)
      .tag(tag)
      .remote_comp(remote_comp)
      .user_context(user_context)
      .buffers(buffers)
      .rbuffers(rbuffers)
      .allow_ok(allow_ok)
      .allow_posted(allow_posted)
      .allow_retry(allow_retry)
      .force_zcopy(force_zcopy)();
}

size_t get_max_bcopy_size_x::call_impl(runtime_t,
                                       packet_pool_t packet_pool) const
{
  // TODO: we can refine the maximum buffer-copy size based on more information
  return packet_pool.p_impl->get_payload_size() - sizeof(tag_t) -
         sizeof(rcomp_t);
}

}  // namespace lci