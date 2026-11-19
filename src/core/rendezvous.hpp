// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#ifndef LCI_CORE_RENDEZVOUS_HPP
#define LCI_CORE_RENDEZVOUS_HPP

namespace lci
{
inline void handle_rdv_rts_common(runtime_t runtime, endpoint_t endpoint,
                                  packet_t* packet,
                                  internal_context_t* rdv_ctx);
// Also for eager protocol
inline void handle_matched_sendrecv(runtime_t runtime, endpoint_t endpoint,
                                    packet_t* packet,
                                    internal_context_t* recv_ctx,
                                    status_t* p_status = nullptr)
{
  if (packet->local_context.is_eager) {
    // information needed from the incoming send packet
    // rank, tag, actual size, packet buffer, eager or rendezvous
    // information needed from the posted recv
    // user buffer, user_context, comp
    comp_t comp = recv_ctx->comp;

    status_t status;
    status.set_done();
    status.buffer = recv_ctx->buffer;
    status.user_context = recv_ctx->user_context;
    delete recv_ctx;
    status.rank = packet->local_context.rank;
    status.tag = packet->local_context.tag;
    if (packet->local_context.size > 0) {
      memcpy(status.buffer, packet->get_payload_address(),
             packet->local_context.size);
    }
    status.size = packet->local_context.size;
    packet->put_back();
    if (!p_status) {
      comp.get_impl()->signal(status);
    } else {
      *p_status = status;
    }
  } else {
    handle_rdv_rts_common(runtime, endpoint, packet, recv_ctx);
    if (p_status) {
      p_status->set_posted();
    }
  }
}

struct rts_msg_t {
  rcomp_t rcomp;
  uintptr_t send_ctx;
  tag_t tag;
  size_t size;
};

struct rtr_msg_t {
  uintptr_t send_ctx;
  uintptr_t recv_ctx_or_key;
  rmr_t rmr;
  size_t offset;
};

// Bit 28 of the RTR immediate data, the first one below the message type
// (bits 29-30): the sender completes the message with writeimm
// (recv_ctx_or_key is an archive key). Otherwise, it completes it with
// write + FIN (recv_ctx_or_key is the recv ctx).
const int RTR_IMM_DATA_WRITEIMM_BIT = 28;

inline void handle_rdv_rts(runtime_t runtime, endpoint_t endpoint,
                           packet_t* packet)
{
  // Extract information from the received RTS packet
  rts_msg_t* rts = reinterpret_cast<rts_msg_t*>(packet->get_payload_address());

  auto entry = runtime.p_impl->default_rhandler_registry.get(rts->rcomp);
  if (entry.type == rhandler_registry_t::type_t::matching_engine) {
    // send recv
    packet->local_context.tag = rts->tag;
    packet->local_context.is_eager = false;

    // get the matching engine
    matching_engine_impl_t* p_matching_engine =
        reinterpret_cast<matching_engine_impl_t*>(entry.value);
    // insert into the matching engine
    auto key = p_matching_engine->make_key(
        packet->local_context.rank, packet->local_context.tag,
        static_cast<matching_policy_t>(entry.metadata));
    auto ret = p_matching_engine->insert(
        key, packet, matching_engine_impl_t::insert_type_t::send);
    if (!ret) return;
    handle_rdv_rts_common(runtime, endpoint, packet,
                          reinterpret_cast<internal_context_t*>(ret));
  } else {
    // am
    handle_rdv_rts_common(runtime, endpoint, packet, nullptr);
  }
}

inline void handle_rdv_rts_common(runtime_t runtime, endpoint_t endpoint,
                                  packet_t* packet, internal_context_t* rdv_ctx)
{
  device_t device = endpoint.get_impl()->device;
  // Extract information from the received RTS packet
  rts_msg_t* rts = reinterpret_cast<rts_msg_t*>(packet->get_payload_address());
  // build the rdv context
  if (!rdv_ctx) {
    // This is an active message
    rdv_ctx = new internal_context_t;
    rdv_ctx->set_user_posted_op(endpoint);
    rdv_ctx->size = rts->size;
    if (rts->size > 0)
      rdv_ctx->buffer = runtime.get_impl()->allocator->allocate_with_mr(
          rts->size, device, &rdv_ctx->mr);
    rdv_ctx->user_context = NULL;
    auto entry = runtime.p_impl->default_rhandler_registry.get(rts->rcomp);
    LCI_DBG_Assert(entry.type == rhandler_registry_t::type_t::comp, "");
    rdv_ctx->comp.p_impl = reinterpret_cast<comp_impl_t*>(entry.value);
  } else {
    // This is a matched send
    // needed from incoming send
    // packet, src_rank, tag
    // needed from posted recv
    // user_data, user_context, comp

    // set rdv_ctx->data size(s) based on rts->size
    LCI_Assert(rdv_ctx->size >= rts->size, "");
    rdv_ctx->size = rts->size;
  }
  rdv_ctx->tag = rts->tag;
  rdv_ctx->rank = packet->local_context.rank;

  // Register the data
  if (rdv_ctx->size > 0 && rdv_ctx->mr.is_empty()) {
    mr_t mr = register_memory_x(rdv_ctx->buffer, rdv_ctx->size)
                  .runtime(runtime)
                  .device(device)();
    rdv_ctx->set_mr_on_the_fly(mr);
  }

  // Prepare the RTR message
  rtr_msg_t rtr;
  rtr.send_ctx = rts->send_ctx;
  rtr.rmr = get_rmr(rdv_ctx->mr);
  rtr.offset = reinterpret_cast<uintptr_t>(rdv_ctx->buffer) - rtr.rmr.base;

  LCI_DBG_Log(LOG_TRACE, "rdv", "send rtr: sctx %p\n", (void*)rtr.send_ctx);

  bool use_writeimm =
      runtime.get_impl()->attr.rdv_protocol == attr_rdv_protocol_t::writeimm;
  imm_tag_archive_t::tag_t writeimm_tag = 0;
  if (use_writeimm) {
    // If all immediate data tags are taken, ask the sender to complete this
    // message with the write protocol instead.
    use_writeimm = runtime.get_impl()->rdv_imm_archive.try_insert(
        reinterpret_cast<uint64_t>(rdv_ctx), &writeimm_tag);
  }
  if (use_writeimm) {
    rtr.recv_ctx_or_key = writeimm_tag;
  } else {
    rtr.recv_ctx_or_key = reinterpret_cast<uintptr_t>(rdv_ctx);
  }

  // send the rtr message
  net_imm_data_t imm_data = set_bits32(0, IMM_DATA_MSG_RTR, 2, 29);
  imm_data = set_bits32(imm_data, use_writeimm, 1, RTR_IMM_DATA_WRITEIMM_BIT);
  error_t error;
  bool succeeded;
  if (sizeof(rtr_msg_t) <=
      device.get_impl()->net_context.get_attr_max_inject_size()) {
    // post_sends copies the message (inline or into the backlog queue), so
    // the rts packet can be returned right away.
    packet->put_back();
    error = endpoint.get_impl()->post_sends((int)rdv_ctx->rank, &rtr,
                                            sizeof(rtr_msg_t), imm_data,
                                            nullptr, false /* allow_retry */);
    succeeded = error.is_done();
  } else {
    // reuse the rts packet as rtr packet
    packet->local_context.local_id = mpmc_set_t::LOCAL_SET_ID_NULL;
    memcpy(packet->get_payload_address(), &rtr, sizeof(rtr_msg_t));
    internal_context_t* rtr_ctx = new internal_context_t;
    rtr_ctx->packet_to_free = packet;
    error = endpoint.get_impl()->post_send(
        (int)rdv_ctx->rank, packet->get_payload_address(), sizeof(rtr_msg_t),
        packet->get_mr(endpoint), imm_data, rtr_ctx, false /* allow_retry */);
    succeeded = error.is_posted();
  }
  if (!succeeded) {
    if (use_writeimm) {
      (void)runtime.get_impl()->rdv_imm_archive.remove(writeimm_tag);
    }
    LCI_Assert(succeeded, "Unexpected error %s\n", error.get_str());
  }
}

inline void handle_rdv_rtr(runtime_t runtime, endpoint_t endpoint,
                           packet_t* packet, net_imm_data_t imm_data)
{
  // the sender side handles the rtr message
  device_t device = endpoint.get_impl()->device;
  net_context_t net_context = device.get_impl()->net_context;
  const size_t max_single_msg_size = net_context.get_attr_max_msg_size();
  rtr_msg_t* rtr = reinterpret_cast<rtr_msg_t*>(packet->get_payload_address());
  internal_context_t* rdv_ctx = (internal_context_t*)rtr->send_ctx;

  auto ectx = new internal_context_extended_t;
  ectx->internal_ctx = rdv_ctx;
  ectx->signal_count =
      (rdv_ctx->size + max_single_msg_size - 1) / max_single_msg_size;
  // The receiver decides the protocol per message (handle_rdv_rts_common).
  const bool use_writeimm = get_bits32(imm_data, 1, RTR_IMM_DATA_WRITEIMM_BIT);
  net_imm_data_t writeimm_data = 0;
  if (use_writeimm) {
    uint32_t tag_bits = runtime.get_impl()->rdv_imm_archive.tag_bits();
    auto recv_key = static_cast<imm_tag_archive_t::tag_t>(rtr->recv_ctx_or_key);
    LCI_Assert(recv_key < (1u << tag_bits),
               "Immediate data tag %u exceeds configured bits %u\n", recv_key,
               tag_bits);
    writeimm_data = set_bits32(0, IMM_DATA_MSG_FIN, 2, 29);
    writeimm_data = set_bits32(writeimm_data, recv_key, tag_bits, 0);
    ectx->recv_ctx = 0;
  } else {
    ectx->recv_ctx = rtr->recv_ctx_or_key;
  }

  void* buffer = rdv_ctx->buffer;
  size_t size = rdv_ctx->size;
  mr_t* p_mr = &rdv_ctx->mr;
  // register the buffer if necessary
  if (p_mr->is_empty()) {
    *p_mr = register_memory_x(buffer, size).runtime(runtime).device(device)();
    rdv_ctx->set_mr_on_the_fly(*p_mr);
  }
  // issue the put/putimm
  if (size > max_single_msg_size) {
    LCI_DBG_Log(LOG_TRACE, "rdv", "Splitting a large message of %lu bytes\n",
                size);
  }
  for (size_t offset = 0; offset < size; offset += max_single_msg_size) {
    char* address = (char*)buffer + offset;
    size_t length = std::min(size - offset, max_single_msg_size);
    bool is_last_chunk = offset + length >= size;
    error_t error;
    if (use_writeimm && is_last_chunk) {
      error = endpoint.get_impl()->post_putImm(
          (int)rdv_ctx->rank, address, length, *p_mr, rtr->offset + offset,
          rtr->rmr, writeimm_data, ectx, false /* allow_retry */);
    } else {
      error = endpoint.get_impl()->post_put(
          (int)rdv_ctx->rank, address, length, *p_mr, rtr->offset + offset,
          rtr->rmr, ectx, false /* allow_retry */);
    }
    LCI_Assert(error.is_posted(), "Unexpected error %s\n", error.get_str());
  }
  // free the rtr packet
  packet->put_back();
}

inline void handle_rdv_local_write(endpoint_t endpoint,
                                   internal_context_extended_t* ectx)
{
  internal_context_t* ctx = ectx->internal_ctx;
  LCI_Assert(ectx->recv_ctx, "Unexpected recv_ctx\n");
  LCI_DBG_Log(LOG_TRACE, "rdv", "send FIN: rctx %p\n", (void*)ectx->recv_ctx);
  net_imm_data_t imm_data = set_bits32(0, IMM_DATA_MSG_FIN, 2, 29);
  error_t error = endpoint.get_impl()->post_sends(
      (int)ctx->rank, &ectx->recv_ctx, sizeof(ectx->recv_ctx), imm_data,
      nullptr, false /* allow_retry */);
  LCI_Assert(error.is_done(), "Unexpected error %s\n", error.get_str());
}

inline void handle_rdv_remote_comp(internal_context_t* ctx)
{
  // We have to count data received by remote put here.
  // if (ctx->data_type == LCI_LONG) {
  //   LCII_PCOUNTER_ADD(net_recv_comp, ctx->data.lbuffer.length);
  // } else {
  //   for (int i = 0; i < ctx->data.iovec.count; ++i)
  //     LCII_PCOUNTER_ADD(net_recv_comp, ctx->data.iovec.lbuffers[i].length);
  // }
  free_ctx_and_signal_comp(ctx);
}

inline void handle_rdv_fin(packet_t* packet)
{
  internal_context_t* ctx;
  memcpy(&ctx, packet->get_payload_address(), sizeof(ctx));
  LCI_DBG_Log(LOG_TRACE, "rdv", "recv FIN: rctx %p\n", ctx);
  packet->put_back();
  handle_rdv_remote_comp(ctx);
}

}  // namespace lci

#endif  // LCI_CORE_RENDEZVOUS_HPP
