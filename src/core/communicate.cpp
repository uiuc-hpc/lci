// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include "lci_internal.hpp"

namespace lci
{
enum class protocol_t {
  none,
  inject,
  eager_bcopy,
  eager_zcopy,
  rdv_zcopy,
  recv,
};

struct post_comm_args_t {
  direction_t direction;
  int rank;
  void* local_buffer;
  size_t size;
  comp_t local_comp;
  runtime_t runtime;
  packet_pool_t packet_pool;
  device_t device;
  endpoint_t endpoint;
  matching_engine_t matching_engine;
  comp_semantic_t comp_semantic;
  mr_t mr;
  uintptr_t remote_disp;
  rmr_t rmr;
  tag_t tag;
  rcomp_t remote_comp;
  void* user_context;
  buffers_t buffers;
  rbuffers_t rbuffers;
  matching_policy_t matching_policy;
  bool allow_done;
  bool allow_posted;
  bool allow_retry;
  bool force_zcopy;
};

struct post_comm_traits_t {
  bool local_buffer_only;
  bool local_comp_only;
  bool is_single_buffer;
  bool is_recv;
  size_t max_inject_size;
  size_t max_bcopy_size;
};

struct post_comm_state_t {
  rcomp_t rhandler = 0;
  bool piggyback_tag_rcomp_in_msg = false;
  net_imm_data_t imm_data = 0;
  packet_t* packet = nullptr;
  void* local_buffer = nullptr;
  size_t size = 0;
  bool user_provided_packet = false;
  internal_context_t* internal_ctx = nullptr;
  protocol_t protocol = protocol_t::none;
  comp_t local_comp = COMP_NULL;
  bool free_local_comp = false;
  status_t status;
  data_t data;
};

void preprocess_args(post_comm_args_t& args)
{
  // handle COMP_NULL and COMP_NULL_RETRY
  if (args.local_comp == COMP_NULL) {
    args.allow_retry = false;
    args.allow_posted = false;
  } else if (args.local_comp == COMP_NULL_RETRY) {
    args.allow_posted = false;
  }

  // handle MR_UNKNOWN
#ifdef LCI_USE_CUDA
  if (args.mr == MR_UNKNOWN) {
    // if the mr is unknown, we need to determine the location of the buffer
    LCI_Assert(args.buffers.empty(),
               "The MR_UNKNOWN should only be used with a single buffer\n");
    accelerator::buffer_attr_t attr =
        accelerator::get_buffer_attr(args.local_buffer);
    if (attr.type == accelerator::buffer_type_t::HOST) {
      // the buffer is in the host memory
      args.mr = MR_HOST;
    } else if (attr.type == accelerator::buffer_type_t::DEVICE) {
      // the buffer is in the device memory
      args.mr = MR_DEVICE;
    } else {
      LCI_Assert(false, "Unknown buffer type %d\n", attr.type);
    }
  }
#endif
}

post_comm_traits_t validate_and_get_traits(const post_comm_args_t& args)
{
  post_comm_traits_t traits;
  traits.local_buffer_only = args.rmr.is_empty() && args.rbuffers.empty();
  traits.local_comp_only = args.remote_comp == 0;
  traits.is_single_buffer = args.buffers.empty();
  traits.is_recv =
      args.direction == direction_t::IN && traits.local_buffer_only;
  traits.max_inject_size =
      args.device.p_impl->net_context.get_attr_max_inject_size();
  traits.max_bcopy_size = get_max_bcopy_size_x()
                              .runtime(args.runtime)
                              .packet_pool(args.packet_pool)();

  // basic checks
  // buffer and buffers should not be used at the same time
  LCI_Assert(args.allow_posted || args.allow_done,
             "At least one of allow_posted and allow_done should be true\n");
  LCI_Assert(args.force_zcopy == false, "force_zcopy is not supported\n");
  if (!args.buffers.empty()) {
    LCI_Assert(args.local_buffer == nullptr,
               "The local buffer should be nullptr\n");
    LCI_Assert(args.size == 0, "The size should be 0\n");
    LCI_Assert(
        args.rbuffers.empty() || args.buffers.size() == args.rbuffers.size(),
        "The number of buffers and rbuffers should be the same\n");
  }
  LCI_Assert(!(args.direction == direction_t::IN && traits.local_buffer_only &&
               !traits.local_comp_only),
             "invalid communication primitive\n");
  LCI_Assert(!(args.direction == direction_t::IN && !traits.local_buffer_only &&
               !traits.local_comp_only),
             "get with signal has not been implemented yet\n");

  return traits;
}

// state in: none
// state out: rhandler
void resolve_rhandler(const post_comm_args_t& args,
                      const post_comm_traits_t& traits,
                      post_comm_state_t& state)
{
  // get the matching engine rhandler
  // remote handler can be the user-specified remote completion handler or the
  // matching engine's remote handler
  state.rhandler = args.remote_comp;
  if (!state.rhandler && !args.matching_engine.is_empty() &&
      traits.local_buffer_only) {
    // this is send-recv (no remote_comp, no remote buffer, matching engine is
    // valid)
    state.rhandler =
        args.matching_engine.get_impl()->get_rhandler(args.matching_policy);
  }
}

// state in: rhandler
// state out: protocol
void set_protocol(const post_comm_args_t& args,
                  const post_comm_traits_t& traits, post_comm_state_t& state)
{
  // determine the message size if we are using the eager protocol
  size_t msg_size_if_eager = args.size;
  if (args.direction == direction_t::OUT && state.rhandler) {
    // send/am/put_signal with eager protocol
    if (args.tag > args.runtime.get_attr_max_imm_tag() ||
        state.rhandler > args.runtime.get_attr_max_imm_rcomp()) {
      msg_size_if_eager += sizeof(args.tag) + sizeof(state.rhandler);
    }
  }

  if (args.direction == direction_t::IN && traits.local_buffer_only) {
    state.protocol = protocol_t::recv;
  } else if (args.direction == direction_t::OUT && traits.local_buffer_only &&
             (msg_size_if_eager > traits.max_bcopy_size ||
              !traits.is_single_buffer)) {
    // We use the rendezvous protocol if
    // 1. we are doing a send/am, and
    // 1.1 The size of the data is larger than the maximum buffer-copy size, or
    // 1.2 We are sending multiple buffers
    state.protocol = protocol_t::rdv_zcopy;
  } else if (traits.is_single_buffer &&
             msg_size_if_eager <= traits.max_inject_size &&
             args.direction == direction_t::OUT &&
             args.comp_semantic == comp_semantic_t::buffer) {
    // We use the inject protocol only if the five conditions are met:
    // 1. We are sending a single buffer, and
    // 2. The size of the data is smaller than the maximum inject size, and
    // 3. The direction is OUT, and
    // 4. The completion type is buffer.
    state.protocol = protocol_t::inject;
  } else if (args.mr.is_empty() && msg_size_if_eager <= traits.max_bcopy_size &&
             traits.is_single_buffer) {
    state.protocol = protocol_t::eager_bcopy;
    if (msg_size_if_eager > args.size) {
      state.piggyback_tag_rcomp_in_msg = true;
    }
  } else {
    state.protocol = protocol_t::eager_zcopy;
  }
}

// state in: none
// state out: local_comp, free_local_comp
void set_local_comp(const post_comm_args_t& args, const post_comm_traits_t&,
                    post_comm_state_t& state)
{
  state.local_comp = args.local_comp;
  state.free_local_comp = false;
  // FIXME: get a more lightweight completion counter.
  // process COMP_BLOCK
  if (state.local_comp == COMP_NULL) {
    state.local_comp = alloc_sync();
    state.free_local_comp = true;
  } else if (state.local_comp == COMP_NULL_RETRY) {
    state.local_comp = alloc_sync();
    state.free_local_comp = true;
  }
}

// state in: protocol, rhandler
// state out: imm_data, piggyback_tag_rcomp_in_msg
void set_immediate_data(const post_comm_args_t& args, const post_comm_traits_t&,
                        post_comm_state_t& state)
{
  // set immediate data
  // immediate data is used for send/am/put with signal
  uint32_t imm_data = 0;
  if (state.protocol == protocol_t::rdv_zcopy) {
    // send/am with rendezvous
    // bit 29-30: imm_data_msg_type_t
    imm_data = set_bits32(0, IMM_DATA_MSG_RTS, 2, 29);
  } else if (args.direction == direction_t::OUT && state.rhandler) {
    // send/am/put_signal with eager protocol
    if (args.tag <= args.runtime.get_attr_max_imm_tag() &&
        state.rhandler <= args.runtime.get_attr_max_imm_rcomp()) {
      // is_fastpath (1) ; rhandler (15) ; tag (16)
      imm_data = set_bits32(imm_data, 1, 1, 31);  // is_fastpath
      imm_data = set_bits32(imm_data, args.tag,
                            args.runtime.get_attr_imm_nbits_tag(), 0);
      imm_data = set_bits32(imm_data, state.rhandler,
                            args.runtime.get_attr_imm_nbits_rcomp(), 16);
    } else {
      // is_fastpath (0) ; msg_type (2)
      static_assert(IMM_DATA_MSG_EAGER == 0, "Unexpected IMM_DATA_MSG_EAGER");
    }
  }
  state.imm_data = imm_data;
}

// state in: protocol, rhandler, piggyback_tag_rcomp_in_msg
// state.out: packet, local_buffer, user_provided_packet
error_t set_packet_if_needed(const post_comm_args_t& args,
                             const post_comm_traits_t& traits,
                             post_comm_state_t& state)
{
  state.local_buffer = args.local_buffer;
  // The bcopy protocol and the rendezvous protocol need a packet
  if (state.protocol != protocol_t::eager_bcopy &&
      state.protocol != protocol_t::rdv_zcopy)
    return errorcode_t::done;
  // get a packet
  if (state.protocol == protocol_t::eager_bcopy &&
      args.packet_pool.p_impl->is_packet(args.local_buffer)) {
    // users provide a packet
    state.user_provided_packet = true;
    state.packet = address2packet(args.local_buffer);
  } else {
    // allocate a packet
    state.packet = args.packet_pool.p_impl->get(!args.allow_retry);
    if (!state.packet) {
      return errorcode_t::retry_nopacket;
    }
  }
  if (state.protocol == protocol_t::rdv_zcopy) {
    return errorcode_t::done;
  }
  // build the packet
  state.local_buffer = state.packet->get_payload_address();
  if (args.direction == direction_t::OUT) {
    if (!state.user_provided_packet)
      memcpy(state.packet->get_payload_address(), args.local_buffer, args.size);
    size_t msg_size = args.size;
    if (state.piggyback_tag_rcomp_in_msg) {
      msg_size += sizeof(args.tag) + sizeof(state.rhandler);
      LCI_DBG_Assert(msg_size <= traits.max_bcopy_size, "");
    }
    if (msg_size > args.runtime.get_attr_packet_return_threshold()) {
      // A lot of data has been written into this packet, which means a
      // large chunk of cache lines have been touched. We should return this
      // packet to the current core's packet pool.
      // TODO: I am skeptical about how much performance gain we can get
      // from this. We should do some experiments to verify this.
      state.packet->local_context.local_id =
          args.packet_pool.get_impl()->get_local_id();
    }
  }
  return errorcode_t::done;
}

// state in: local_buffer, piggyback_tag_rcomp_in_msg
// state out: size
void piggyback_tag_rcomp(const post_comm_args_t& args,
                         const post_comm_traits_t& traits,
                         post_comm_state_t& state)
{
  state.size = args.size;
  if (state.piggyback_tag_rcomp_in_msg) {
    LCI_DBG_Assert(state.size <= traits.max_bcopy_size, "");
    memcpy((char*)state.local_buffer + state.size, &args.tag, sizeof(args.tag));
    state.size += sizeof(args.tag);
    memcpy((char*)state.local_buffer + state.size, &state.rhandler,
           sizeof(state.rhandler));
    state.size += sizeof(state.rhandler);
  }
}

// state in: protocol
// state out: none
error_t check_backlog(const post_comm_args_t& args, const post_comm_traits_t&,
                      post_comm_state_t& state)
{
  // We need to directly return retry if
  // 1. allow_retry is true
  // 2. the endpoint's backlog queue is not empty
  // 3. we are not doing a recv
  if (!args.endpoint.get_impl()->is_backlog_queue_empty(args.rank) &&
      args.allow_retry && state.protocol != protocol_t::recv) {
    LCI_PCOUNTER_ADD(retry_due_to_backlog_queue, 1);
    return errorcode_t::retry_backlog;
  }
  return errorcode_t::done;
}

// state in: none
// state out: data
void set_data(const post_comm_args_t& args, const post_comm_traits_t& traits,
              post_comm_state_t& state)
{
  data_t data;
  if (traits.is_single_buffer) {
    data = data_t(buffer_t(args.local_buffer, args.size, args.mr));
  } else {
    data = data_t(args.buffers);
  }
  state.data = data;
}

// state in: protocol, packet, local_comp, data
// state out: internal_ctx
void set_internal_ctx(const post_comm_args_t& args, const post_comm_traits_t&,
                      post_comm_state_t& state)
{
  state.internal_ctx = new internal_context_t;
  state.internal_ctx->rank = args.rank;
  state.internal_ctx->tag = args.tag;
  state.internal_ctx->user_context = args.user_context;
  state.internal_ctx->data = state.data;
  state.internal_ctx->packet_to_free = state.packet;
  // We need to set the local completion object in one of the following cases:
  // 1. The protocol is zero-copy.
  // 2. The completion type is network.
  // 3. The direction is IN (we are doing a recv/get).
  // In other words, eager send/am/put will immediately complete.
  if (state.protocol == protocol_t::eager_zcopy ||
      state.protocol == protocol_t::rdv_zcopy ||
      args.comp_semantic == comp_semantic_t::network ||
      args.direction == direction_t::IN) {
    state.internal_ctx->comp = state.local_comp;
  }
  // We need to have valid memeory regions if all of the following conditions
  // are met:
  // 1. The protocol is zero-copy.
  // Note: mr for zero-copy send/recv will be handled in the rendezvous
  // protocol.
  if (state.protocol == protocol_t::eager_zcopy) {
    state.internal_ctx->mr_on_the_fly =
        register_data(state.internal_ctx->data, args.device);
    state.data = state.internal_ctx->data;
  }
}

// state in: data
// state out: status
void set_status(const post_comm_args_t& args, const post_comm_traits_t&,
                post_comm_state_t& state)
{
  status_t status;
  status.set_done();
  status.rank = args.rank;
  status.tag = args.tag;
  status.user_context = args.user_context;
  status.data = state.data;
  state.status = status;
}

// state in: all
// state out: status
error_t post_network_op(const post_comm_args_t& args,
                        const post_comm_traits_t& traits,
                        post_comm_state_t& state)
{
  error_t error;
  if (args.direction == direction_t::OUT) {
    /**********************************************************************************
     * direction out
     **********************************************************************************/
    if (state.protocol == protocol_t::inject) {
      // inject protocol (return retry or done)
      if (traits.local_buffer_only) {
        error = args.endpoint.p_impl->post_sends(args.rank, args.local_buffer,
                                                 state.size, state.imm_data,
                                                 args.allow_retry);
      } else if (!state.rhandler) {
        // rdma write
        error = args.endpoint.p_impl->post_puts(args.rank, args.local_buffer,
                                                state.size, args.remote_disp,
                                                args.rmr, args.allow_retry);
      } else {
        // rdma write with immediate data
        error = args.endpoint.p_impl->post_putImms(
            args.rank, args.local_buffer, state.size, args.remote_disp,
            args.rmr, state.imm_data, args.allow_retry);
      }
      // end of inject protocol
    } else if (state.protocol == protocol_t::eager_bcopy) {
      // buffer-copy protocol
      if (traits.local_buffer_only) {
        // buffer-copy send
        // note: we need to use state.size instead of args.size
        error = args.endpoint.p_impl->post_send(
            args.rank, state.local_buffer, state.size,
            state.packet->get_mr(args.device), state.imm_data,
            state.internal_ctx, args.allow_retry);
      } else if (!state.rhandler) {
        // buffer-copy put
        error = args.endpoint.p_impl->post_put(
            args.rank, state.local_buffer, state.size,
            state.packet->get_mr(args.device), args.remote_disp, args.rmr,
            state.internal_ctx, args.allow_retry);
      } else {
        // buffer-copy put with signal
        error = args.endpoint.p_impl->post_putImm(
            args.rank, state.local_buffer, state.size,
            state.packet->get_mr(args.device), args.remote_disp, args.rmr,
            state.imm_data, state.internal_ctx, args.allow_retry);
      }
      if (error.is_posted() && args.comp_semantic == comp_semantic_t::buffer) {
        error = errorcode_t::done;
      }
      // end of bcopy protocol
    } else if (state.protocol == protocol_t::eager_zcopy) {
      data_t& data = state.data;
      if (traits.local_buffer_only) {
        // zero-copy send
        error = args.endpoint.p_impl->post_send(
            args.rank, data.buffer.base, data.buffer.size, data.buffer.mr,
            state.imm_data, state.internal_ctx, args.allow_retry);
      } else {
        // zero-copy put
        if (data.is_buffer()) {
          if (!state.rhandler) {
            error = args.endpoint.p_impl->post_put(
                args.rank, data.buffer.base, data.buffer.size, data.buffer.mr,
                args.remote_disp, args.rmr, state.internal_ctx,
                args.allow_retry);
          } else {
            error = args.endpoint.p_impl->post_putImm(
                args.rank, data.buffer.base, data.buffer.size, data.buffer.mr,
                args.remote_disp, args.rmr, state.imm_data, state.internal_ctx,
                args.allow_retry);
          }
        } else {
          auto extended_ctx = new internal_context_extended_t;
          extended_ctx->internal_ctx = state.internal_ctx;
          extended_ctx->signal_count = data.get_buffers_count();
          if (state.rhandler) {
            extended_ctx->imm_data_rank = args.rank;
            extended_ctx->imm_data = state.imm_data;
          }
          bool allow_retry = args.allow_retry;
          for (size_t i = 0; i < data.buffers.count; i++) {
            if (i > 0) allow_retry = false;
            error = args.endpoint.p_impl->post_put(
                args.rank, data.buffers.buffers[i].base,
                data.buffers.buffers[i].size, data.buffers.buffers[i].mr,
                args.rbuffers[i].disp, args.rbuffers[i].rmr, extended_ctx,
                allow_retry);
            if (i == 0 && error.is_retry()) {
              delete extended_ctx;
              return error;
            }
            LCI_Assert(error.is_posted(), "Unexpected error %d\n", error);
          }
        }
        // end of zero-copy put
      }
    } else /* protocol == protocol_t::rdv_zcopy */ {
      // rendezvous send
      // build the rts message
      internal_context_t* rts_ctx = nullptr;
      data_t& data = state.internal_ctx->data;
      size_t rts_size = rts_msg_t::get_size(data);
      rts_msg_t* p_rts;
      if (rts_size <= traits.max_inject_size) {
        p_rts = reinterpret_cast<rts_msg_t*>(malloc(rts_size));
      } else {
        LCI_Assert(rts_size <= traits.max_bcopy_size,
                   "The rts message is too large\n");
        p_rts = static_cast<rts_msg_t*>(state.packet->get_payload_address());
        rts_ctx = new internal_context_t;
        rts_ctx->packet_to_free = state.packet;
      }
      p_rts->send_ctx = (uintptr_t)state.internal_ctx;
      p_rts->rdv_type = rdv_type_t::single;
      p_rts->tag = args.tag;
      p_rts->rcomp = state.rhandler;
      if (traits.is_single_buffer) {
        p_rts->load_buffer(state.size);
      } else {
        p_rts->load_buffers(args.buffers);
      }
      // post send for the rts message
      if (rts_size <= traits.max_inject_size) {
        error = args.endpoint.p_impl->post_sends(
            args.rank, p_rts, rts_size, state.imm_data, args.allow_retry);
        free(p_rts);
      } else {
        error = args.endpoint.p_impl->post_send(
            args.rank, p_rts, rts_size, state.packet->get_mr(args.device),
            state.imm_data, rts_ctx, args.allow_retry);
      }
      if (error.is_retry()) {
        delete rts_ctx;
      } else if (error.is_done()) {
        error = errorcode_t::posted;
      }
      // end of rendezvous send
      // end of zero-copy protocol
    }
    // end of direction out
  } else {
    /**********************************************************************************
     * direction in
     **********************************************************************************/
    if (traits.local_buffer_only) {
      // recv
      error = errorcode_t::posted;
      LCI_DBG_Assert(state.internal_ctx->packet_to_free == nullptr,
                     "recv does not need a packet!\n");
      // get the matching policy
      // If any of the LCI_ANY is used, we will ignore the matching policy
      matching_policy_t matching_policy = args.matching_policy;
      if (args.rank == ANY_SOURCE && args.tag == ANY_TAG) {
        matching_policy = matching_policy_t::none;
      } else if (args.rank == ANY_SOURCE) {
        matching_policy = matching_policy_t::tag_only;
      } else if (args.tag == ANY_TAG) {
        matching_policy = matching_policy_t::rank_only;
      }
      auto key = args.matching_engine.get_impl()->make_key(args.rank, args.tag,
                                                           matching_policy);
      auto ret = args.matching_engine.get_impl()->insert(
          key, state.internal_ctx, matching_engine_impl_t::insert_type_t::recv);
      if (ret) {
        handle_matched_sendrecv(args.runtime, args.endpoint,
                                reinterpret_cast<packet_t*>(ret),
                                state.internal_ctx, &state.status);
        error = errorcode_t::done;
      }
    } else /* !local_buffer_only */ {
      // get
      if (state.protocol == protocol_t::eager_bcopy) {
        // buffer-copy
        error = args.endpoint.p_impl->post_get(
            args.rank, state.local_buffer, state.size,
            state.packet->get_mr(args.device), args.remote_disp, args.rmr,
            state.internal_ctx, args.allow_retry);
      } else {
        // zero-copy
        data_t& data = state.internal_ctx->data;
        if (data.is_buffer()) {
          error = args.endpoint.p_impl->post_get(
              args.rank, data.buffer.base, data.buffer.size, data.buffer.mr,
              args.remote_disp, args.rmr, state.internal_ctx, args.allow_retry);
        } else {
          auto extended_ctx = new internal_context_extended_t;
          extended_ctx->internal_ctx = state.internal_ctx;
          extended_ctx->signal_count = data.get_buffers_count();
          bool allow_retry = args.allow_retry;
          for (size_t i = 0; i < data.buffers.count; i++) {
            if (i > 0) allow_retry = false;
            error = args.endpoint.p_impl->post_get(
                args.rank, data.buffers.buffers[i].base,
                data.buffers.buffers[i].size, data.buffers.buffers[i].mr,
                args.rbuffers[i].disp, args.rbuffers[i].rmr, extended_ctx,
                allow_retry);
            if (i == 0 && error.is_retry()) {
              delete extended_ctx;
              return error;
            } else {
              LCI_Assert(error.is_posted(), "Unexpected error %d\n", error);
            }
          }
        }
      }
    }
    // end of direction in
  }
  return error;
}

void exit_handler(error_t error_, const post_comm_args_t& args,
                  post_comm_state_t& state)
{
  state.status.error = error_;
  if (state.protocol != protocol_t::recv) {
    if (state.status.is_posted()) {
      LCI_Assert(!state.internal_ctx->comp.is_empty(),
                 "Internal error: the internal context comp object is empty "
                 "while the operation is posted\n");
    } else if (state.status.is_done()) {
      LCI_Assert(!state.internal_ctx || state.internal_ctx->comp.is_empty(),
                 "Internal error: the internal context comp object is not "
                 "empty while the operation is done\n");
    }
  }
  if (state.status.is_retry()) {
    LCI_DBG_Assert(args.allow_retry, "Unexpected retry\n");
    if (state.internal_ctx && state.internal_ctx->mr_on_the_fly) {
      deregister_data(state.internal_ctx->data);
    }
    if (!state.user_provided_packet && state.packet) {
      state.packet->put_back();
    }
    delete state.internal_ctx;
  }
  if (state.status.is_posted() && !args.allow_posted) {
    while (!sync_test(state.local_comp, &state.status)) {
      progress_x()
          .runtime(args.runtime)
          .device(args.device)
          .endpoint(args.endpoint)();
    }
    state.status.set_done();
  }
  if (state.free_local_comp) {
    free_comp(&state.local_comp);
  }
  if (state.status.is_done() && !args.allow_done) {
    lci::comp_signal(state.local_comp, state.status);
    state.status.set_posted();
  }
  if (state.status.is_done()) {
    LCI_PCOUNTER_ADD(communicate_ok, 1);
  } else if (state.status.is_posted()) {
    LCI_PCOUNTER_ADD(communicate_posted, 1);
  } else {
    switch (state.status.error.errorcode) {
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
}

status_t post_comm_x::call_impl(
    direction_t direction, int rank, void* local_buffer, size_t size,
    comp_t local_comp, runtime_t runtime, packet_pool_t packet_pool,
    device_t device, endpoint_t endpoint, matching_engine_t matching_engine,
    comp_semantic_t comp_semantic, mr_t mr, uintptr_t remote_disp, rmr_t rmr,
    tag_t tag, rcomp_t remote_comp, void* user_context, buffers_t buffers,
    rbuffers_t rbuffers, matching_policy_t matching_policy, bool allow_done,
    bool allow_posted, bool allow_retry, bool force_zcopy) const
{
  error_t error;
  post_comm_args_t args = {
      direction,     rank,         local_buffer, size,        local_comp,
      runtime,       packet_pool,  device,       endpoint,    matching_engine,
      comp_semantic, mr,           remote_disp,  rmr,         tag,
      remote_comp,   user_context, buffers,      rbuffers,    matching_policy,
      allow_done,    allow_posted, allow_retry,  force_zcopy,
  };
  preprocess_args(args);
  post_comm_traits_t traits = validate_and_get_traits(args);
  post_comm_state_t state;
  // state out: rhandler
  resolve_rhandler(args, traits, state);
  // state in: rhandler
  // state out: protocol
  set_protocol(args, traits, state);
  // state in: protocol
  // state out: none
  error = check_backlog(args, traits, state);
  if (!error.is_done()) goto exit;
  // state in: protocol, rhandler
  // state out: imm_data, piggyback_tag_rcomp_in_msg
  set_immediate_data(args, traits, state);
  // state in: protocol, rhandler, piggyback_tag_rcomp_in_msg
  // state.out: packet, local_buffer, user_provided_packet
  error = set_packet_if_needed(args, traits, state);
  if (!error.is_done()) goto exit;
  // state in: local_buffer, piggyback_tag_rcomp_in_msg
  // state out: size
  piggyback_tag_rcomp(args, traits, state);
  // state in: none
  // state out: local_comp, free_local_comp
  set_local_comp(args, traits, state);
  // state in: none
  // state out: data
  set_data(args, traits, state);
  // state in: data
  // state out: status
  set_status(args, traits, state);
  // state in: protocol, packet, local_comp, data
  // state out: internal_ctx
  if (state.protocol != protocol_t::inject) {
    set_internal_ctx(args, traits, state);
  }
  // state in: all
  // state out: status
  error = post_network_op(args, traits, state);

exit:
  exit_handler(error, args, state);
  return state.status;
}

status_t post_am_x::call_impl(int rank, void* local_buffer, size_t size,
                              comp_t local_comp, rcomp_t remote_comp,
                              runtime_t runtime, packet_pool_t packet_pool,
                              device_t device, endpoint_t endpoint,
                              comp_semantic_t comp_semantic, mr_t mr, tag_t tag,
                              void* user_context, buffers_t buffers,
                              bool allow_done, bool allow_posted,
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
      .allow_done(allow_done)
      .allow_posted(allow_posted)
      .allow_retry(allow_retry)
      .force_zcopy(force_zcopy)();
}

status_t post_send_x::call_impl(
    int rank, void* local_buffer, size_t size, tag_t tag, comp_t local_comp,
    runtime_t runtime, packet_pool_t packet_pool, device_t device,
    endpoint_t endpoint, matching_engine_t matching_engine,
    comp_semantic_t comp_semantic, mr_t mr, void* user_context,
    buffers_t buffers, matching_policy_t matching_policy, bool allow_done,
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
      .allow_done(allow_done)
      .allow_posted(allow_posted)
      .allow_retry(allow_retry)
      .force_zcopy(force_zcopy)();
}

status_t post_recv_x::call_impl(int rank, void* local_buffer, size_t size,
                                tag_t tag, comp_t local_comp, runtime_t runtime,
                                packet_pool_t packet_pool, device_t device,
                                endpoint_t endpoint,
                                matching_engine_t matching_engine, mr_t mr,
                                void* user_context, buffers_t buffers,
                                matching_policy_t matching_policy,
                                bool allow_done, bool allow_posted,
                                bool allow_retry, bool force_zcopy) const
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
      .allow_done(allow_done)
      .allow_posted(allow_posted)
      .allow_retry(allow_retry)
      .force_zcopy(force_zcopy)();
}

status_t post_put_x::call_impl(
    int rank, void* local_buffer, size_t size, comp_t local_comp,
    uintptr_t remote_disp, rmr_t rmr, runtime_t runtime,
    packet_pool_t packet_pool, device_t device, endpoint_t endpoint,
    comp_semantic_t comp_semantic, mr_t mr, tag_t tag, rcomp_t remote_comp,
    void* user_context, buffers_t buffers, rbuffers_t rbuffers, bool allow_done,
    bool allow_posted, bool allow_retry, bool force_zcopy) const
{
  return post_comm_x(direction_t::OUT, rank, local_buffer, size, local_comp)
      .remote_disp(remote_disp)
      .rmr(rmr)
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
      .allow_done(allow_done)
      .allow_posted(allow_posted)
      .allow_retry(allow_retry)
      .force_zcopy(force_zcopy)();
}

status_t post_get_x::call_impl(int rank, void* local_buffer, size_t size,
                               comp_t local_comp, uintptr_t remote_disp,
                               rmr_t rmr, runtime_t runtime,
                               packet_pool_t packet_pool, device_t device,
                               endpoint_t endpoint, mr_t mr, tag_t tag,
                               rcomp_t remote_comp, void* user_context,
                               buffers_t buffers, rbuffers_t rbuffers,
                               bool allow_done, bool allow_posted,
                               bool allow_retry, bool force_zcopy) const
{
  return post_comm_x(direction_t::IN, rank, local_buffer, size, local_comp)
      .remote_disp(remote_disp)
      .rmr(rmr)
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
      .allow_done(allow_done)
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