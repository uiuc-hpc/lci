// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

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

const char* get_protocol_str(protocol_t protocol)
{
  static const char protocol_str[][16] = {
      "none", "inject", "eager_bcopy", "eager_zcopy", "rdv_zcopy", "recv",
  };
  return protocol_str[static_cast<int>(protocol)];
}

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
  matching_policy_t matching_policy;
  bool allow_done;
  bool allow_posted;
  bool allow_retry;
};

struct post_comm_traits_t {
  bool local_buffer_only;
  bool local_comp_only;
  bool is_recv;
  size_t max_inject_size;
  size_t max_bcopy_size;
};

struct post_comm_state_t {
  rcomp_t rhandler = 0;
  bool piggyback_tag_rcomp_in_msg = false;
  net_imm_data_t imm_data = 0;
  packet_t* packet = nullptr;
  size_t packet_size_to_send = 0;
  bool user_provided_packet = false;
  internal_context_t* internal_ctx = nullptr;
  protocol_t protocol = protocol_t::none;
  mr_t mr;
  comp_t local_comp = COMP_NULL;
  bool comp_passed_to_network = false;
  status_t status;
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
#endif  // LCI_USE_CUDA
}

post_comm_traits_t validate_and_get_traits(const post_comm_args_t& args)
{
  post_comm_traits_t traits;
  traits.local_buffer_only = args.rmr.is_empty();
  traits.local_comp_only = args.remote_comp == 0;
  traits.is_recv =
      args.direction == direction_t::IN && traits.local_buffer_only;
  traits.max_inject_size =
      args.device.p_impl->net_context.get_attr_max_inject_size();
  traits.max_bcopy_size = get_max_bcopy_size_x()
                              .runtime(args.runtime)
                              .packet_pool(args.packet_pool)();

  // basic checks
  LCI_Assert(args.allow_posted || args.allow_done,
             "At least one of allow_posted and allow_done should be true\n");
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
// state out: protocol, piggyback_tag_rcomp_in_msg
void set_protocol(const post_comm_args_t& args,
                  const post_comm_traits_t& traits, post_comm_state_t& state)
{
  bool force_zcopy = false;
#ifdef LCI_USE_CUDA
  if (args.mr == MR_DEVICE ||
      (!args.mr.is_empty() && args.mr.get_impl()->acc_attr.type ==
                                  accelerator::buffer_type_t::DEVICE)) {
    force_zcopy = true;
  }
#endif  // LCI_USE_CUDA
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
             (msg_size_if_eager > traits.max_bcopy_size || force_zcopy)) {
    // We use the rendezvous protocol if
    // 1. we are doing a send/am, and
    // 1.1 The size of the data is larger than the maximum buffer-copy size, or
    // 1.2 We force the use of the zero-copy protocol.
    state.protocol = protocol_t::rdv_zcopy;
  } else if (msg_size_if_eager <= traits.max_inject_size &&
             args.direction == direction_t::OUT &&
             args.comp_semantic == comp_semantic_t::memory && !force_zcopy) {
    // We use the inject protocol only if the five conditions are met:
    // 1. We are sending a single buffer, and
    // 2. The size of the data is smaller than the maximum inject size, and
    // 3. The direction is OUT, and
    // 4. The completion type is buffer.
    // 5. We are not forcing the use of the zero-copy protocol.
    state.protocol = protocol_t::inject;
  } else if (args.mr.is_empty() && msg_size_if_eager <= traits.max_bcopy_size &&
             !force_zcopy) {
    state.protocol = protocol_t::eager_bcopy;
    if (msg_size_if_eager > args.size) {
      state.piggyback_tag_rcomp_in_msg = true;
    }
  } else {
    state.protocol = protocol_t::eager_zcopy;
  }
}

// state in: none
// state out: local_comp
void set_local_comp(const post_comm_args_t& args, const post_comm_traits_t&,
                    post_comm_state_t& state)
{
  state.local_comp = args.local_comp;
  // process COMP_BLOCK
  if (!args.allow_posted) {
    state.local_comp = alloc_sync();
  }
}

// state in: protocol, rhandler
// state out: imm_data
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
// state.out: packet, user_provided_packet
error_t set_packet_if_needed(const post_comm_args_t& args,
                             [[maybe_unused]] const post_comm_traits_t& traits,
                             post_comm_state_t& state)
{
  // Only the bcopy protocol and the rendezvous protocol need a packet
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
  if (args.direction == direction_t::OUT) {
    if (!state.user_provided_packet)
      memcpy(state.packet->get_payload_address(), args.local_buffer, args.size);
    state.packet_size_to_send = args.size;
    if (state.piggyback_tag_rcomp_in_msg) {
      char* buffer = (char*)state.packet->get_payload_address();
      memcpy((char*)buffer + state.packet_size_to_send, &args.tag,
             sizeof(args.tag));
      state.packet_size_to_send += sizeof(args.tag);
      memcpy((char*)buffer + state.packet_size_to_send, &state.rhandler,
             sizeof(state.rhandler));
      state.packet_size_to_send += sizeof(state.rhandler);
      LCI_Assert(state.packet_size_to_send <= traits.max_bcopy_size, "");
    }
    if (state.packet_size_to_send >
        args.runtime.get_attr_packet_return_threshold()) {
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

// state in: protocol, packet, local_comp
// state out: internal_ctx, mr
void set_internal_ctx(const post_comm_args_t& args, const post_comm_traits_t&,
                      post_comm_state_t& state)
{
  state.internal_ctx = new internal_context_t;
  state.internal_ctx->set_user_posted_op(args.endpoint);
  state.internal_ctx->rank = args.rank;
  state.internal_ctx->tag = args.tag;
  state.internal_ctx->user_context = args.user_context;
  state.internal_ctx->buffer = args.local_buffer;
  state.internal_ctx->size = args.size;
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
    LCI_Assert(!state.local_comp.is_empty(),
               "Local completion object is empty\n");
    state.internal_ctx->comp = state.local_comp;
    state.comp_passed_to_network = true;
  } else {
    state.comp_passed_to_network = false;
  }
  // We need to have valid memeory regions if all of the following conditions
  // are met:
  // 1. The protocol is zero-copy.
  // Note: mr for zero-copy send/recv will be handled in the rendezvous
  // protocol.
  state.mr = args.mr;
  if (state.protocol == protocol_t::eager_zcopy && state.mr.is_empty()) {
    state.mr = register_memory_x(args.local_buffer, args.size)
                   .runtime(args.runtime)
                   .device(args.device)();
    state.internal_ctx->set_mr_on_the_fly(state.mr);
  }
}

// state in: none
// state out: status
void set_status(const post_comm_args_t& args, const post_comm_traits_t&,
                post_comm_state_t& state)
{
  status_t status;
  status.rank = args.rank;
  status.tag = args.tag;
  status.user_context = args.user_context;
  status.buffer = args.local_buffer;
  status.size = args.size;
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
        error = args.endpoint.p_impl->post_sends(
            args.rank, args.local_buffer, args.size, state.imm_data,
            state.internal_ctx, args.allow_retry);
      } else if (!state.rhandler) {
        // rdma write
        error = args.endpoint.p_impl->post_puts(
            args.rank, args.local_buffer, args.size, args.remote_disp, args.rmr,
            state.internal_ctx, args.allow_retry);
      } else {
        // rdma write with immediate data
        error = args.endpoint.p_impl->post_putImms(
            args.rank, args.local_buffer, args.size, args.remote_disp, args.rmr,
            state.imm_data, state.internal_ctx, args.allow_retry);
      }
      // end of inject protocol
    } else if (state.protocol == protocol_t::eager_bcopy) {
      // buffer-copy protocol
      void* buffer = state.packet->get_payload_address();
      if (traits.local_buffer_only) {
        // buffer-copy send
        // note: we need to use state.size instead of args.size
        error = args.endpoint.p_impl->post_send(
            args.rank, buffer, state.packet_size_to_send,
            state.packet->get_mr(args.device), state.imm_data,
            state.internal_ctx, args.allow_retry);
      } else if (!state.rhandler) {
        // buffer-copy put
        error = args.endpoint.p_impl->post_put(
            args.rank, buffer, state.packet_size_to_send,
            state.packet->get_mr(args.device), args.remote_disp, args.rmr,
            state.internal_ctx, args.allow_retry);
      } else {
        // buffer-copy put with signal
        error = args.endpoint.p_impl->post_putImm(
            args.rank, buffer, state.packet_size_to_send,
            state.packet->get_mr(args.device), args.remote_disp, args.rmr,
            state.imm_data, state.internal_ctx, args.allow_retry);
      }
      if (error.is_posted() && args.comp_semantic == comp_semantic_t::memory) {
        error = errorcode_t::done;
      }
      // end of bcopy protocol
    } else if (state.protocol == protocol_t::eager_zcopy) {
      if (traits.local_buffer_only) {
        // zero-copy send
        error = args.endpoint.p_impl->post_send(
            args.rank, args.local_buffer, args.size, state.mr, state.imm_data,
            state.internal_ctx, args.allow_retry);
      } else {
        // zero-copy put
        if (!state.rhandler) {
          error = args.endpoint.p_impl->post_put(
              args.rank, args.local_buffer, args.size, state.mr,
              args.remote_disp, args.rmr, state.internal_ctx, args.allow_retry);
        } else {
          error = args.endpoint.p_impl->post_putImm(
              args.rank, args.local_buffer, args.size, state.mr,
              args.remote_disp, args.rmr, state.imm_data, state.internal_ctx,
              args.allow_retry);
        }
        // end of zero-copy put
      }
    } else /* protocol == protocol_t::rdv_zcopy */ {
      // rendezvous send
      // build the rts message
      rts_msg_t* p_rts;
      if (sizeof(rts_msg_t) <= traits.max_inject_size) {
        p_rts = reinterpret_cast<rts_msg_t*>(malloc(sizeof(rts_msg_t)));
      } else {
        LCI_Assert(sizeof(rts_msg_t) <= traits.max_bcopy_size,
                   "The rts message is too large\n");
        p_rts = static_cast<rts_msg_t*>(state.packet->get_payload_address());
      }
      p_rts->send_ctx = (uintptr_t)state.internal_ctx;
      p_rts->tag = args.tag;
      p_rts->rcomp = state.rhandler;
      p_rts->size = args.size;
      // post send for the rts message
      if (sizeof(rts_msg_t) <= traits.max_inject_size) {
        error = args.endpoint.p_impl->post_sends(
            args.rank, p_rts, sizeof(rts_msg_t), state.imm_data, nullptr,
            args.allow_retry);
        free(p_rts);
      } else {
        error = args.endpoint.p_impl->post_send(
            args.rank, p_rts, sizeof(rts_msg_t),
            state.packet->get_mr(args.device), state.imm_data, nullptr,
            args.allow_retry);
      }
      if (error.is_done()) {
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
    if (state.protocol == protocol_t::recv) {
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
        error = state.status.error;
      }
    } else {
      // get
      if (state.protocol == protocol_t::eager_bcopy) {
        // buffer-copy
        error = args.endpoint.p_impl->post_get(
            args.rank, state.packet->get_payload_address(), args.size,
            state.packet->get_mr(args.device), args.remote_disp, args.rmr,
            state.internal_ctx, args.allow_retry);
      } else {
        // zero-copy
        error = args.endpoint.p_impl->post_get(
            args.rank, args.local_buffer, args.size, state.mr, args.remote_disp,
            args.rmr, state.internal_ctx, args.allow_retry);
      }
    }
    // end of direction in
  }
  return error;
}

void exit_handler(error_t error_, const post_comm_args_t& args,
                  post_comm_state_t& state)
{
  // We should not access to the internal context after this point
  // if the error is not retry, as it might be freed by the network progress
  // function
  state.status.error = error_;
  if (state.protocol != protocol_t::recv) {
    if (state.status.is_done()) {
      LCI_Assert(!state.comp_passed_to_network,
                 "Internal error: the internal context comp object is passed "
                 "to network while the operation is done\n");
    } else if (state.status.is_posted()) {
      LCI_Assert(state.comp_passed_to_network,
                 "Internal error: the internal context comp object is not "
                 "passed to the network while the operation is posted\n");
    }
  }
  if (state.status.is_retry()) {
    LCI_DBG_Assert(args.allow_retry, "Unexpected retry\n");
    if (state.user_provided_packet)
      // If users provided a packet and they need to retry, we should not free
      // it.
      state.internal_ctx->packet_to_free = nullptr;
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
  if (!args.allow_posted) {
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
    comp_t local_comp, runtime_t runtime, device_t device, endpoint_t endpoint,
    packet_pool_t packet_pool, matching_engine_t matching_engine,
    comp_semantic_t comp_semantic, mr_t mr, uintptr_t remote_disp, rmr_t rmr,
    tag_t tag, rcomp_t remote_comp, void* user_context,
    matching_policy_t matching_policy, bool allow_done, bool allow_posted,
    bool allow_retry) const
{
  error_t error;
  post_comm_args_t args = {
      direction,     rank,         local_buffer,    size,       local_comp,
      runtime,       packet_pool,  device,          endpoint,   matching_engine,
      comp_semantic, mr,           remote_disp,     rmr,        tag,
      remote_comp,   user_context, matching_policy, allow_done, allow_posted,
      allow_retry,
  };
  preprocess_args(args);
  post_comm_traits_t traits = validate_and_get_traits(args);
  post_comm_state_t state;
  // state out: rhandler
  resolve_rhandler(args, traits, state);
  // state in: rhandler
  // state out: protocol, piggyback_tag_rcomp_in_msg
  set_protocol(args, traits, state);
  // state in: protocol
  // state out: none
  error = check_backlog(args, traits, state);
  if (!error.is_done()) goto exit;
  // state in: protocol, rhandler, piggyback_tag_rcomp_in_msg
  // state.out: packet, user_provided_packet
  error = set_packet_if_needed(args, traits, state);
  if (!error.is_done()) goto exit;
  // state in: protocol, rhandler
  // state out: imm_data
  set_immediate_data(args, traits, state);
  // state in: none
  // state out: local_comp
  set_local_comp(args, traits, state);
  // state in: none
  // state out: status
  set_status(args, traits, state);
  // state in: protocol, packet, local_comp
  // state out: internal_ctx, mr
  set_internal_ctx(args, traits, state);
  // state in: all
  // state out: status
  error = post_network_op(args, traits, state);

exit:
  exit_handler(error, args, state);
  return state.status;
}

status_t post_am_x::call_impl(int rank, void* local_buffer, size_t size,
                              comp_t local_comp, rcomp_t remote_comp,
                              runtime_t runtime, device_t device,
                              endpoint_t endpoint, packet_pool_t packet_pool,
                              comp_semantic_t comp_semantic, mr_t mr, tag_t tag,
                              void* user_context, bool allow_done,
                              bool allow_posted, bool allow_retry) const
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
      .allow_done(allow_done)
      .allow_posted(allow_posted)
      .allow_retry(allow_retry)();
}

status_t post_send_x::call_impl(
    int rank, void* local_buffer, size_t size, tag_t tag, comp_t local_comp,
    runtime_t runtime, device_t device, endpoint_t endpoint,
    packet_pool_t packet_pool, matching_engine_t matching_engine,
    comp_semantic_t comp_semantic, mr_t mr, void* user_context,
    matching_policy_t matching_policy, bool allow_done, bool allow_posted,
    bool allow_retry) const
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
      .matching_policy(matching_policy)
      .allow_done(allow_done)
      .allow_posted(allow_posted)
      .allow_retry(allow_retry)();
}

status_t post_recv_x::call_impl(
    int rank, void* local_buffer, size_t size, tag_t tag, comp_t local_comp,
    runtime_t runtime, device_t device, endpoint_t endpoint,
    packet_pool_t packet_pool, matching_engine_t matching_engine,
    comp_semantic_t comp_semantic, mr_t mr, void* user_context,
    matching_policy_t matching_policy, bool allow_done, bool allow_posted,
    bool allow_retry) const
{
  return post_comm_x(direction_t::IN, rank, local_buffer, size, local_comp)
      .runtime(runtime)
      .packet_pool(packet_pool)
      .device(device)
      .endpoint(endpoint)
      .matching_engine(matching_engine)
      .comp_semantic(comp_semantic)
      .mr(mr)
      .tag(tag)
      .user_context(user_context)
      .matching_policy(matching_policy)
      .allow_done(allow_done)
      .allow_posted(allow_posted)
      .allow_retry(allow_retry)();
}

status_t post_put_x::call_impl(int rank, void* local_buffer, size_t size,
                               comp_t local_comp, uintptr_t remote_disp,
                               rmr_t rmr, runtime_t runtime, device_t device,
                               endpoint_t endpoint, packet_pool_t packet_pool,
                               comp_semantic_t comp_semantic, mr_t mr,
                               tag_t tag, rcomp_t remote_comp,
                               void* user_context, bool allow_done,
                               bool allow_posted, bool allow_retry) const
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
      .allow_done(allow_done)
      .allow_posted(allow_posted)
      .allow_retry(allow_retry)();
}

status_t post_get_x::call_impl(int rank, void* local_buffer, size_t size,
                               comp_t local_comp, uintptr_t remote_disp,
                               rmr_t rmr, runtime_t runtime, device_t device,
                               endpoint_t endpoint, packet_pool_t packet_pool,
                               comp_semantic_t comp_semantic, mr_t mr,
                               tag_t tag, rcomp_t remote_comp,
                               void* user_context, bool allow_done,
                               bool allow_posted, bool allow_retry) const
{
  return post_comm_x(direction_t::IN, rank, local_buffer, size, local_comp)
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
      .allow_done(allow_done)
      .allow_posted(allow_posted)
      .allow_retry(allow_retry)();
}

size_t get_max_bcopy_size_x::call_impl(runtime_t,
                                       packet_pool_t packet_pool) const
{
  // TODO: we can refine the maximum buffer-copy size based on more information
  return packet_pool.p_impl->get_payload_size() - sizeof(tag_t) -
         sizeof(rcomp_t);
}

}  // namespace lci
